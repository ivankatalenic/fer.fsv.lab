#pragma once

#include <string>
#include <memory>
#include <stdexcept>

#include "cnf.hh"
#include "decision.hh"

void print_cnf(const CNF& cnf) {
	for (const auto& clause : cnf.clauses) {
		for (const auto& literal : clause.literals) {
			int id{literal.id};
			if (literal.is_negative) {
				id = -id;
			}
			std::cout << id << " || ";
		}
		std::cout << std::endl << "&&" << std::endl;
	}
}

std::string format_dec_node(const std::shared_ptr<decision> node) {
	int value{node->literal.id};
	if (node->literal.is_negative) {
		value = -value;
	}
	std::string ret(std::to_string(value));
	if (!node->literal.is_negative) {
		ret = std::string(" ") + ret;
	}
	if (node->is_implicit) {
		ret += " [I]";
	} else {
		ret += " [ ]";
	}
	if (node->is_conflict) {
		ret += "[C]";
	} else {
		ret += "[ ]";
	}
	return ret;
}

void write_n(const int n, const char c) {
	if (n < 0) {
		throw std::invalid_argument("write_n: n can't be negative");
	}
	for (int i{0}; i < n; i++) {
		std::cout.put(c);
	}
}

void print_decision_graph_rec(const std::shared_ptr<decision> subtree, int indentation) {
	write_n(indentation, '\t'); std::cout << format_dec_node(subtree) << std::endl;
	indentation++;
	for (int i{0}; i < 2; i++) {
		if (subtree->children[i] == nullptr) {
			continue;
		}
		print_decision_graph_rec(subtree->children[i], indentation);
	}
}

void print_decision_graph(const std::shared_ptr<decision> root) {
	print_decision_graph_rec(root, 0);
	std::cout.flush();
}

void print_model(const std::unordered_set<int>& model) {
	std::cout << "model: " << std::endl;
	for (auto var : model) {
		std::cout << var << ", " << std::endl;
	}
}

void print_literal(Literal l) {
	if (l.is_negative) {
		std::cout << "-";
	}
	std::cout << l.id << std::endl;
	std::cout.flush();
}
