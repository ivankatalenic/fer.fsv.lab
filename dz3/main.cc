#include <iostream>
#include <fstream>
#include <streambuf>
#include <sstream>
#include <limits>
#include <cstdio>
#include <stdexcept>
#include <string>
#include <vector>
#include <cctype>
#include <memory>
#include <utility>
#include <unordered_set>
#include <unordered_map>

#include "cnf.hh"
#include "print.hh"
#include "decision.hh"

std::shared_ptr<decision> ucp_propagate_literal(std::shared_ptr<decision> current_decision, Literal unit_literal) {
	std::shared_ptr<decision> new_decision{std::make_shared<decision>(current_decision, unit_literal)};
	current_decision->add_child(new_decision);
	new_decision->is_implicit = true;
	
	// Create a new (simplified) state (CNF) for that decision
	for (const auto& clause : current_decision->cnf.clauses) {
		const int idx{clause.get_literal_idx(unit_literal.id)};
		if (idx == -1) {
			// This clause doesn't contain that literal
			// This clause is unaffected
			new_decision->cnf.add_clause(clause);
			continue;
		}
		if (clause.literal_count() == 1 && clause.literals[idx].is_negative != unit_literal.is_negative) {
			// This is a conflict with another clause
			// Two unit clauses with different signs!
			new_decision->is_conflict = true;
			return new_decision;
		}
		if (clause.literal_count() == 1) {
			// This is the same unit clause (with the same sign)
			// Don't include a true clause into further consideration
			continue;
		}
		if (clause.literals[idx].is_negative == unit_literal.is_negative) {
			// This single literal is true in this clause, therefore the clause is true and out of further concern
			continue;
		}
		Clause truncated_clause;
		for (std::size_t i{0}; i < clause.literal_count(); i++) {
			if (static_cast<int>(i) == idx) {
				// Remove the false literal from consideration
				continue;
			}
			truncated_clause.add_literal(clause.literals[i]);
		}
		if (truncated_clause.literal_count() == 0) {
			throw std::runtime_error("ucp: propagate: we removed a false literal from a clause and got an empty clause!");
		}
		new_decision->cnf.add_clause(truncated_clause);
	}

	return new_decision;
}

std::pair<Literal, bool> find_unit_literal(const std::vector<Clause>& clauses) {
	for (const auto& clause : clauses) {
		if (clause.literal_count() == 1) {
			return std::make_pair(clause.literals[0], true);
		}
	}
	return std::make_pair(Literal(), false);
}

// Unit clause propagation
// Condition: There can't exist a clause with a single *assigned* literal.
// Condition: A clause can't contain more that one literal of a single variable.
std::shared_ptr<decision> unit_clause_propagation(std::shared_ptr<decision> current_decision) {
	if (current_decision == nullptr) {
		throw std::runtime_error("ucp: received a nil pointer");
	}
	std::shared_ptr<decision> initial_decision{current_decision};

	while (true) {
		auto [unit_literal, is_unit_literal_found]{find_unit_literal(current_decision->cnf.clauses)};
		if (!is_unit_literal_found) {
			break;
		}

		// Propagate the unit clause
		std::shared_ptr<decision> new_decision{ucp_propagate_literal(current_decision, unit_literal)};
		if (new_decision->is_conflict) {
			initial_decision->is_conflict = true;
			// Prune the graph, free the memory
			initial_decision->remove_children();
			return initial_decision;
		}

		current_decision = new_decision;
	}

	return current_decision;
}

// Selects a next literal to assign based on the current decision.
// Tries to explore a sibling literal if there is a conflict with the current one.
// Condition: The input decision must not have both children explored.
Literal select_next_literal(std::shared_ptr<decision> current) {
	if (current == nullptr) {
		throw std::runtime_error("selecting a next literal: current decision is null");
	}
	if (current->children[0] != nullptr && current->children[1] != nullptr) {
		throw std::runtime_error("selecting a next literal: both children are explored");
	}
	if (current->is_conflict) {
		// Try selecting a negative (sibling) literal
		Literal sibling_literal{current->literal};
		sibling_literal.is_negative = !sibling_literal.is_negative;
		return sibling_literal;
	}
	// Select the first not explored literal
	return current->cnf.clauses[0].literals[0];
}

std::shared_ptr<decision> assignment_decision(std::shared_ptr<decision> parent_dec, Literal literal) {
	std::shared_ptr<decision> new_dec{std::make_shared<decision>(parent_dec, literal)};
	parent_dec->add_child(new_dec);

	// Adjust (minimize) the state (CNF)
	for (const auto& clause : parent_dec->cnf.clauses) {
		const int idx{clause.get_literal_idx(literal.id)};
		if (idx == -1) {
			new_dec->cnf.add_clause(clause);
			continue;
		}
		if (clause.literals[idx].is_negative == literal.is_negative) {
			// The clause is true, remove it from further consideration
			continue;
		}
		if (clause.literal_count() == 1) {
			// The clause is false, conflict
			new_dec->is_conflict = true;
			return new_dec;
		}
		// Remove the false literal from further consideration
		Clause truncated_clause;
		for (std::size_t i{0}; i < clause.literal_count(); i++) {
			if (static_cast<int>(i) == idx) {
				// Remove the false literal
				continue;
			}
			truncated_clause.add_literal(clause.literals[i]);
		}
		if (truncated_clause.literal_count() == 0) {
			throw std::runtime_error("assignment: we removed a false literal from a clause and got an empty clause!");
		}
		new_dec->cnf.add_clause(truncated_clause);
	}
	return new_dec;
}

bool is_sibling_explored(std::shared_ptr<decision> current_decision) {
	std::shared_ptr<decision> parent{current_decision->parent};
	return parent->children[0] != nullptr && parent->children[1] != nullptr;
}

// Backtracks to the first decision with an unexplored sibling, or to the root of the decision tree.
std::shared_ptr<decision> backtrack_conflict(std::shared_ptr<decision> dec) {
	while (dec->parent != nullptr && (is_sibling_explored(dec) || dec->is_implicit)) {
		if (dec->is_implicit && dec->is_conflict) {
			dec->parent->is_conflict = true;
		}	

		dec = dec->parent;

		if (dec->children[0] != nullptr && dec->children[1] != nullptr
			&& dec->children[0]->is_conflict && dec->children[1]->is_conflict) {
			// Both children/siblings are in conflict
			// Set the parent as in conflict
			dec->is_conflict = true;
		}

		// Prune the graph, free the memory
		dec->remove_children();
	}
	return dec;
}

enum class pure_literal_case {negative, positive, conflict};

pure_literal_case pure_literal(const Literal l) {
	if (l.is_negative) {
		return pure_literal_case::negative;
	}
	return pure_literal_case::positive;
}

std::pair<int, pure_literal_case> find_pure_literal(const CNF& cnf) {
	std::unordered_map<int, pure_literal_case> state;
	for (const auto& clause : cnf.clauses) {
		for (const auto& literal : clause.literals) {
			if (!state.contains(literal.id)) {
				state[literal.id] = pure_literal(literal);
				continue;
			}
			if (state[literal.id] == pure_literal(literal)) {
				continue;
			}
			state[literal.id] = pure_literal_case::conflict;
		}
	}
	for (const auto& [id, value] : state) {
		if (value != pure_literal_case::conflict) {
			return std::make_pair(id, value);
		}
	}
	// A pure literal is not found
	return std::make_pair(0, pure_literal_case::conflict);
}

std::shared_ptr<decision> pure_literal_removal(std::shared_ptr<decision> current) {
	while (true) {
		auto [id, value]{find_pure_literal(current->cnf)};
		if (value == pure_literal_case::conflict) {
			// A pure literal is not found
			break;
		}

		Literal pure_literal(id, value == pure_literal_case::negative);

		std::shared_ptr<decision> new_dec{std::make_shared<decision>(current, pure_literal)};
		current->add_child(new_dec);
		new_dec->is_implicit = true;

		for (const auto& clause : current->cnf.clauses) {
			if (clause.get_literal_idx(pure_literal.id) != -1) {
				// The clause contains the pure literal, therefore the clause is true
				// Remove the clause
				continue;
			}
			// The clause doesn't contain the pure literal.
			new_dec->cnf.add_clause(clause);
		}

		current = new_dec;
	}

	return current;
}

std::size_t count_nodes(std::shared_ptr<decision> root) {
	if (root == nullptr) {
		return 0;
	}
	return count_nodes(root->children[0]) + count_nodes(root->children[1]) + 1;
}

// DPLL SAT solver
// If the resulting decision is is_conflict, then there isn't a solution.
std::shared_ptr<decision> dpll(const CNF& cnf) {
	std::shared_ptr<decision> root{std::make_shared<decision>()};
	root->cnf = cnf;

	std::shared_ptr<decision> current_decision{unit_clause_propagation(root)};
	if (current_decision->is_conflict) {
		return root;
	}

	current_decision = pure_literal_removal(current_decision);

	std::size_t iteration{0};
	while (current_decision->cnf.clause_count() > 0 || current_decision->is_conflict) {
		iteration++;
		if (iteration % 200 == 0) {
			const auto node_count{count_nodes(root)};
			std::cout << "node count: " << node_count << std::endl;
		}

		Literal selected_literal;
		if (!current_decision->is_conflict) {
			// Not a conflict!
			selected_literal = select_next_literal(current_decision);
		} else if (!is_sibling_explored(current_decision)) {
			// Conflict, and a sibling literal ISN'T explored!
			// Explore the sibling literal.
			selected_literal = current_decision->literal;
			selected_literal.is_negative = !selected_literal.is_negative;
			current_decision = current_decision->parent;
		} else {
			// Conflict, and a sibling literal IS explored!
			// 1. Backtrack to the first decision with an unexplored SIBLING, and that isn't implicit decision.
			current_decision = backtrack_conflict(current_decision);
			if (current_decision == root) {
				// Every decision leads to a conflict!
				// A model doesn't exist for the input CNF!
				return root;
			}
			// 2. Explore the sibling literal.
			selected_literal = current_decision->literal;
			selected_literal.is_negative = !selected_literal.is_negative;
			current_decision = current_decision->parent;
		}

		current_decision = assignment_decision(current_decision, selected_literal);
		if (current_decision->is_conflict) {
			continue;
		}
		
		current_decision = unit_clause_propagation(current_decision);
		if (current_decision->is_conflict) {
			continue;
		}

		current_decision = pure_literal_removal(current_decision);
	}

	return root;
}

std::unordered_set<int> get_variables(const CNF& cnf) {
	std::unordered_set<int> variables;
	for (const auto& clause : cnf.clauses) {
		for (const auto& literal : clause.literals) {
			variables.insert(literal.id);
		}
	}
	return variables;
}

std::shared_ptr<decision> get_next_model_node(std::shared_ptr<decision> dec) {
	if (dec == nullptr || dec->children_count() == 0) {
		return nullptr;
	}
	if (dec->children_count() == 1) {
		auto child{dec->children[0] != nullptr ? dec->children[0] : dec->children[1]};
		if (child->is_conflict) {
			throw std::runtime_error("encountered a model node that is in conflict");
		}
		return child;
	}
	if (!dec->children[0]->is_conflict && !dec->children[1]->is_conflict) {
		throw std::runtime_error("encountered a model node that has both valid paths");
	}
	if (dec->children[0]->is_conflict && dec->children[1]->is_conflict) {
		throw std::runtime_error("encountered a model node that has both conflicting paths");
	}
	return !dec->children[0]->is_conflict ? dec->children[0] : dec->children[1];
}

std::unordered_set<int> get_model(std::shared_ptr<decision> dec) {
	std::unordered_set<int> model(get_variables(dec->cnf));
	dec = get_next_model_node(dec);
	while (dec != nullptr) {
		model.erase(dec->literal.id);
		model.insert(dec->literal.is_negative ? -dec->literal.id : dec->literal.id);

		dec = get_next_model_node(dec);
	}
	return model;
}

int main(int argc, char* argv[]) {
	if (argc != 2) {
		throw std::length_error("there isn't exactly one program argument");
	}
	const std::string input_filename(argv[1]);

	CNF cnf{import_from_file(input_filename)};

	std::shared_ptr<decision> result{dpll(cnf)};
	if (result->is_conflict) {
		std::cout << "conflict" << std::endl;
		return EXIT_SUCCESS;
	}

	// print_decision_graph(result);
	auto model{get_model(result)};
	print_model(model);

	return EXIT_SUCCESS;
}
