#include <fstream>
#include <iostream>

#include <nlohmann/json.hpp>

#include <yostat/parse.hpp>

// Determine which modules in a design are primitives
std::set<std::string> unique_primitives_in_design(nlohmann::json &j);

Design *read_json(std::string path) {
  // Try and open the input file
  std::ifstream file_ifstream;
  file_ifstream.open(path);

  // Check if we failed to open the file
  if (file_ifstream.fail()) {
    return nullptr;
  }

  // Parse to a json object
  nlohmann::json j;
  file_ifstream >> j;

  // Extract the primitives used in this design
  std::set<std::string> device_primitives = unique_primitives_in_design(j);

  // Now pull out all our useful data
  std::string top_module = "top";
  std::map<std::string, YosysModule> modules;
  for (auto module_it = j["modules"].begin(); module_it != j["modules"].end();
       ++module_it) {
    // Create a struct for this module
    YosysModule mod;
    mod.name = module_it.key();

    // Check if this is the top module?
    auto &attributes = module_it.value()["attributes"];
    auto attr_top = attributes.find("top");
    if (attr_top != attributes.end()) {
      top_module = mod.name;
    }

    // Load the number of cells used by the module
    auto &cells = module_it.value()["cells"];
    for (auto cells_it = cells.begin(); cells_it != cells.end(); ++cells_it) {
      mod.increment_celltype(cells_it.value()["type"]);
    }
    modules[mod.name] = mod;
  }

  Module *tree =
      generate_module_tree(modules, device_primitives, nullptr, top_module);
  Design *d = new Design;
  d->top = tree;
  d->primitives = unique_primitives_in_tree(tree);
  return d;
}

bool YosysModule::all_cells_are_primitives(std::set<std::string> primitives) {
  for (auto &cell : cell_counts) {
    if (primitives.find(cell.first) == primitives.end()) {
      return false;
    }
  }
  return true;
}

std::set<std::string> unique_primitives_in_design(nlohmann::json &j) {
  std::set<std::string> primitives;

  // Iterate all modules and check the 'blackbox'/'whitebox' attribute as a
  // proxy for them being a device primitive
  for (auto module_it = j["modules"].begin(); module_it != j["modules"].end();
       ++module_it) {
    auto &attributes = module_it.value()["attributes"];
    auto blackbox = attributes.find("blackbox");
    auto whitebox = attributes.find("whitebox");
    // Does the blackbox/whitebox attribute exist?
    if (blackbox != attributes.end() || whitebox != attributes.end()) {
      primitives.emplace(module_it.key());
    }
  }

  return primitives;
}

// Get the primitives that actually showed up in the design so that we don't
// display a bunch of empty columns
std::vector<std::string> unique_primitives_in_tree(Module *tree) {
  // Create quick lambda to do the recursive work for us
  std::function<void(const Module *, std::set<std::string> &)> visitor =
      [&visitor](const Module *m, std::set<std::string> &uniq_primitives) {
        for (auto &prim : m->primitives) {
          uniq_primitives.emplace(prim.first);
        }
        for (const Module *child : m->submodules) {
          visitor(child, uniq_primitives);
        }
      };

  // Iterate all modules in the tree
  std::set<std::string> uniq_primitives;
  visitor(tree, uniq_primitives);

  // Convert to vector
  std::vector<std::string> ret(uniq_primitives.begin(), uniq_primitives.end());
  std::sort(ret.begin(), ret.end());
  return ret;
}

void delete_module_tree(Module *m) {
  // Recursively delete the children, then ourselves
  for (auto submodule : m->submodules) {
    delete_module_tree(submodule);
  }
  delete (m);
}

Module *generate_module_tree(std::map<std::string, YosysModule> modules,
                             std::set<std::string> primitive_names,
                             Module *parent, std::string module_name) {
  // Look up the data we pulled from the json earlier
  auto yosys_mod = modules[module_name];

  // Instantiate the module for the wx data tree
  Module *mod = new Module(parent, module_name);

  // In order to differentiate logic used by a module and logic used by
  // submodules of that module, create a special 'self' submodule for modules
  // that do not consist entirely of primitives
  Module *mod_self_primitives = nullptr;
  if (!yosys_mod.all_cells_are_primitives(primitive_names)) {
    mod_self_primitives = new Module(mod, " (self)");
  }

  // For each cell this module contains, recurse to build them out
  for (auto &cell : yosys_mod.cell_counts) {
    // If it isn't a primitive, recurse and build it out properly
    // For non-primitives with multiple instances, we generate two hierarchy
    // levels - one the counts all instantiations as one line item, and then
    // each individual instantiation below that. This way, we can easily see
    // both the individual and combined weight of the modules.
    const bool is_primitive =
        primitive_names.find(cell.first) != primitive_names.end();
    if (!is_primitive) {
      if (cell.second > 1) {
        // Multi-instance case
        // Create the multi-instance holder
        Module *holder = new Module(mod, "[" + std::to_string(cell.second) +
                                             "x] " + cell.first);
        // Generate each individual instance tree using the holder as a parent
        for (int i = 0; i < cell.second; i++) {
          generate_module_tree(modules, primitive_names, holder, cell.first);
        }
        // Tally the holder primitive counts
        for (auto *submod : holder->submodules) {
          for (auto &prim : submod->primitives) {
            holder->increment_primitive_count(prim.first, prim.second);
          }
        }
      } else {
        generate_module_tree(modules, primitive_names, mod, cell.first);
      }
    } else {
      // If it is a primitive, just update the counter for it
      if (mod_self_primitives) {
        mod_self_primitives->set_primitive_count(cell.first, cell.second);
      } else {
        mod->set_primitive_count(cell.first, cell.second);
      }
    }
  }

  // Now that we have built the children for this node, iterate them and sum
  // their primitive counts to get our own primitive count
  for (auto *submod : mod->submodules) {
    for (auto &prim : submod->primitives) {
      mod->increment_primitive_count(prim.first, prim.second);
    }
  }

  return mod;
}
