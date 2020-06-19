#include <fstream>

#include <nlohmann/json.hpp>

#include <yostat/parse.hpp>

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

  std::map<std::string, YosysModule> modules;

  // Now pull out all our useful data
  for (auto module_it = j["modules"].begin(); module_it != j["modules"].end();
       ++module_it) {
    YosysModule mod;
    mod.name = module_it.key();
    auto &cells = module_it.value()["cells"];
    for (auto cells_it = cells.begin(); cells_it != cells.end(); ++cells_it) {
      mod.increment_celltype(cells_it.value()["type"]);
    }
    modules[mod.name] = mod;
  }

  Module *tree = generate_module_tree(modules, nullptr, "top");
  Design *d = new Design;
  d->top = tree;
  d->primitives = unique_primitives_in_tree(tree);
  return d;
}

bool is_primitive(std::string cell) {
  return ECP5_PRIMITIVES.find(cell) != ECP5_PRIMITIVES.end();
}

bool YosysModule::all_cells_are_primitives() {
  for (auto &cell : cell_counts) {
    if (!is_primitive(cell.first))
      return false;
  }
  return true;
}

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
                             Module *parent, std::string module_name) {
  // Look up the data we pulled from the json earlier
  auto yosys_mod = modules[module_name];

  // Instantiate the module for the wx data tree
  Module *mod = new Module(parent, module_name);

  // In order to differentiate logic used by a module and logic used by
  // submodules of that module, create a special 'self' submodule for modules
  // that do not consist entirely of primitives
  Module *mod_self_primitives = nullptr;
  if (!yosys_mod.all_cells_are_primitives()) {
    mod_self_primitives = new Module(mod, " (self)");
  }

  // For each cell this module contains, recurse to build them out
  for (auto &cell : yosys_mod.cell_counts) {
    // If it isn't a primitive, recurse and build it out properly
    // For non-primitives with multiple instances, we generate two hierarchy
    // levels - one the counts all instantiations as one line item, and then
    // each individual instantiation below that. This way, we can easily see
    // both the individual and combined weight of the modules.
    if (!is_primitive(cell.first)) {
      if (cell.second > 1) {
        // Multi-instance case
        // Create the multi-instance holder
        Module *holder = new Module(mod, "[" + std::to_string(cell.second) +
                                             "x] " + cell.first);
        // Generate each individual instance tree using the holder as a parent
        for (int i = 0; i < cell.second; i++) {
          generate_module_tree(modules, holder, cell.first);
        }
        // Tally the holder primitive counts
        for (auto *submod : holder->submodules) {
          for (auto &prim : submod->primitives) {
            holder->increment_primitive_count(prim.first, prim.second);
          }
        }
      } else {
        generate_module_tree(modules, mod, cell.first);
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
