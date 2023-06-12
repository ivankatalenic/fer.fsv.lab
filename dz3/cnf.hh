#pragma once

#include <vector>

class Literal {
public:
	Literal(): id{0}, is_negative{false} {}
	Literal(int id, bool is_negative = false): id{id}, is_negative{is_negative} {}
	
	int id;
	bool is_negative;
};

class Clause {
public:
	void add_literal(Literal l) {
		literals.push_back(l);
	}
	std::size_t literal_count() const {
		return literals.size();
	}
	// Returns -1 when a literal is not found.
	int get_literal_idx(const int id) const {
		for (std::size_t i{0}; i < literals.size(); i++) {
			if (literals[i].id == id) {
				return i;
			}
		}
		return -1;
	}

	std::vector<Literal> literals;
};

class CNF {
public:
	void add_clause(Clause c) {
		clauses.push_back(c);
	}
	std::size_t clause_count() const {
		return clauses.size();
	}

	int variable_count{0};
	std::vector<Clause> clauses;
};

CNF import_from_file(const std::string& filename) {
	std::ifstream input_stream(filename);
	if (!input_stream.good()) {
		throw std::invalid_argument("can't open a file");
	}

	// Clearing the comments from the input file
	for (int c{input_stream.get()}; c == 'c'; c = input_stream.get()) {
		input_stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
	}
	input_stream.unget();

	// Process the problem line
	// Remove the 'p' sign
	input_stream.ignore();
	// Remove the space
	for (int c{input_stream.get()}; std::isspace(c); c = input_stream.get());
	input_stream.unget();
	// Extract the format
	std::stringbuf format_buf;
	for (int c{input_stream.get()}; !std::isspace(c); c = input_stream.get()) {
		format_buf.sputc(static_cast<char>(c));
	}
	input_stream.unget();
	const std::string format(format_buf.str());

	std::cout << "format: " << format << std::endl;

	int variable_count;
	input_stream >> variable_count;
	int clause_count;
	input_stream >> clause_count;

	std::cout << "variables: " << variable_count << ", clauses: " << clause_count << std::endl;

	input_stream.ignore(std::numeric_limits<std::streamsize>::max(), '\n');

	CNF cnf;
	cnf.variable_count = variable_count;
	Clause current_clause;
	while (true) {
		int variable_id;
		input_stream >> variable_id;
		if (!input_stream.good()) {
			break;
		}
		
		if (variable_id == 0) {
			cnf.add_clause(current_clause);
			current_clause = Clause();
			continue;
		}

		bool is_negative{false};
		if (variable_id < 0) {
			is_negative = true;
			variable_id = -variable_id;
		}

		current_clause.add_literal(Literal(variable_id, is_negative));
	}
	if (current_clause.literal_count() > 0) {
		cnf.add_clause(current_clause);
	}

	return cnf;
}
