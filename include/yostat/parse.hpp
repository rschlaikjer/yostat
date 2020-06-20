#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

struct YosysModule {
  // Basic struct for tracking some data we pull from the yosys output json for
  // each module. To tally the resource usage, we only really care about
  // - The name of each module
  // - The number of cells it has of each type
  std::string name;
  std::map<std::string, int> cell_counts;
  void increment_celltype(std::string celltype) {
    auto search = cell_counts.find(celltype);
    if (search == cell_counts.end()) {
      cell_counts[celltype] = 0;
    }
    cell_counts[celltype] = cell_counts[celltype] + 1;
  }
  bool all_cells_are_primitives(std::set<std::string> primitive_names);
};

// One element in the data view control.
// The design is represented in wx as a tree, where each node in the tree has
// the number of primitives used by that node _and_ all child nodes.
// Due to the way that wx interacts with the data structures, we need to retain
// pointers from parents to children, as well as children to parents.
struct Module {
  // Construct a module with no parent. Only the top module should be created
  // this way.
  Module(std::string name_) : parent(nullptr), name(name_) {}
  // Create a module that is a child of another module.
  Module(Module *parent_, std::string name_) : parent(parent_), name(name_) {
    if (parent)
      parent->add_submodule(this);
  }

  // Invoked on parents to connect child modules
  void add_submodule(Module *s) { submodules.emplace_back(s); }

  // Directly set the primitive count for a certain primitive
  void set_primitive_count(std::string prim, int count) {
    primitives[prim] = count;
  }

  // Increment the current count on this module for a given primitive
  // If the primitive was previously unreferenced, increment starts from zero.
  void increment_primitive_count(std::string prim, int count) {
    auto search = primitives.find(prim);
    if (search == primitives.end()) {
      primitives[prim] = count;
      return;
    }
    primitives[prim] += count;
  }

  // Get the number of primitives of a given type used by this node (which
  // includes all resources used by child nodes)
  int get_primitive_count(std::string primitive) {
    auto search = primitives.find(primitive);
    if (search == primitives.end()) {
      return 0;
    } else {
      return search->second;
    }
  }

  Module *parent;
  std::string name;
  std::map<std::string, int> primitives;
  std::vector<Module *> submodules;
};

// Recursive generator for Module tree
Module *generate_module_tree(std::map<std::string, YosysModule> modules,
                             std::set<std::string> primitive_names,
                             Module *parent, std::string module_name);
// Module tree destructor
void delete_module_tree(Module *m);

// Wrapper class that contains all the data necessary for wx to display a design
struct Design {
  ~Design() { delete_module_tree(top); }
  std::vector<std::string> primitives;
  Module *top;
};

// Load a design from a yosys json file
Design *read_json(std::string path);

// Get the set of primitives actually used by a design
std::vector<std::string> unique_primitives_in_tree(Module *tree);
