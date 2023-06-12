#pragma once

static int child_index(bool is_negative) {
	if (is_negative) {
		return 0;
	}
	return 1;
}

// Every decision node can have one complementary node. The node and the complementary node share the same parent.
// The complementary node has to have the negated literal of the reference node.
class decision {
public:
	decision(): is_implicit{false}, is_conflict{false}, literal(), parent{nullptr}, children{nullptr, nullptr} {
	}
	
	decision(std::shared_ptr<decision> parent, Literal literal):
		is_implicit{false}, is_conflict{false}, literal{literal}, parent{parent}, children{nullptr, nullptr} {
	}
	
	void add_child(std::shared_ptr<decision> child) {
		this->children[child_index(child->literal.is_negative)] = child;
	}

	void remove_children() {
		// Deleting a reference to the parent
		if (children[0] != nullptr) {
			children[0]->parent.reset();
			children[0]->remove_children();
		}
		if (children[1] != nullptr) {
			children[1]->parent.reset();
			children[1]->remove_children();
		}

		// Deleting references to the children
		children[0].reset();
		children[1].reset();
	}

	std::size_t children_count() const {
		std::size_t ret{0};
		if (children[0] != nullptr) {
			ret++;
		}
		if (children[1] != nullptr) {
			ret++;
		}
		return ret;
	}

	// Returns a single child. Whether there is more than one child.
	std::pair<std::shared_ptr<decision>, bool> get_child() const {
		if (children[0] != nullptr && children[1] != nullptr) {
			return std::make_pair(children[0], true);
		}
		if (children[0] != nullptr) {
			return std::make_pair(children[0], false);
		}
		return std::make_pair(children[1], false);
	}

	// Is the assignment implicit (through Unit clause propagation or Pure literal elimination)
	bool is_implicit;
	// Does the decision lead to a conflict
	bool is_conflict;

	Literal literal;

	std::shared_ptr<decision> parent;
	std::shared_ptr<decision> children[2];

	CNF cnf;
};
