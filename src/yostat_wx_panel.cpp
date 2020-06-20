#include <functional>
#include <map>
#include <string>
#include <vector>

#include <yostat/yostat_wx_panel.hpp>

enum Ids {
  RELOAD_FILE = 100,
};

/* clang-format off */
BEGIN_EVENT_TABLE(YostatWxPanel, wxFrame)
EVT_DATAVIEW_ITEM_ACTIVATED(wxID_ANY, YostatWxPanel::on_dataview_item_activated)
EVT_MENU(Ids::RELOAD_FILE, YostatWxPanel::reload)
END_EVENT_TABLE()
/* clang-format on */

void YostatWxPanel::on_dataview_item_activated(wxDataViewEvent &evt) {
  // On doubleclick / enter, toggle expansion of container items
  wxDataViewItem item = evt.GetItem();
  if (!item.IsOk())
    return;
  if (evt.GetModel()->IsContainer(item)) {
    if (_dataview->IsExpanded(item)) {
      _dataview->Collapse(item);
    } else {
      _dataview->Expand(item);
    }
  }
}

bool YostatDataModel::HasContainerColumns(const wxDataViewItem &item) const {
  return true;
}

bool YostatDataModel::IsContainer(const wxDataViewItem &item) const {
  Module *node = reinterpret_cast<Module *>(item.GetID());
  if (!node) {
    return true;
  }
  return node->submodules.size() > 0;
}

wxDataViewItem YostatDataModel::GetParent(const wxDataViewItem &item) const {
  if (!item.IsOk()) {
    // Invisible root has no parent
    return wxDataViewItem(nullptr);
  }

  Module *node = reinterpret_cast<Module *>(item.GetID());
  return wxDataViewItem((void *)node->parent);
}

unsigned int YostatDataModel::GetColumnCount() const {
  return _design->primitives.size() + 1;
}

wxString YostatDataModel::GetColumnType(unsigned int col) const {
  if (col == 0) {
    return wxT("string");
  }
  return wxT("long");
}

unsigned int YostatDataModel::GetChildren(const wxDataViewItem &item,
                                          wxDataViewItemArray &children) const {
  // If the item is the root, return our top model
  Module *node = reinterpret_cast<Module *>(item.GetID());
  if (!node) {
    children.Add(wxDataViewItem((void *)_design->top));
    return 1;
  }

  // Otherwise, get the actual node children
  for (auto *submodule : node->submodules) {
    children.Add(wxDataViewItem((void *)submodule));
  }
  return node->submodules.size();
}

void YostatDataModel::GetValue(wxVariant &variant, const wxDataViewItem &item,
                               unsigned int col) const {
  Module *node = reinterpret_cast<Module *>(item.GetID());
  if (col == 0) {
    variant = node->name;
    return;
  }

  variant = (long)node->get_primitive_count(_design->primitives[col - 1]);
}

bool YostatDataModel::SetValue(const wxVariant &variant,
                               const wxDataViewItem &item, unsigned int col) {
  return false;
}

Design *YostatDataModel::get_design() { return _design; }

void YostatDataModel::set_design(Design *d) {
  // Need to iteratively compare new design and old design and try to update in
  // place as much as possible to preserve current view state
  std::function<void(Module *, Module *)> update_module = [&](Module *m_old,
                                                              Module *m_new) {
    // Update the name and primitive counts directly
    m_old->name = m_new->name;
    m_old->primitives = m_new->primitives;

    // For the submodules, there are three cases:
    // - Submodule on m_new present on m_old
    //  -> Recurse directly
    // - Submodule on m_new not present on m_old
    //  -> Detatch submodule from m_new and attach to m_old
    // - Submodule on m_old not present on m_new
    //  -> Delete submodule from m_old

    // Take a copy of the old submodules
    std::vector<Module *> old_submodules = m_old->submodules;
    // Then clear the list so that we can readd as necessary
    m_old->submodules.clear();
    for (auto new_it = m_new->submodules.begin();
         new_it != m_new->submodules.end();) {
      Module *new_submodule = *new_it;

      // Try and match this submodule against the old submodules list
      bool did_update_in_place = false;
      for (auto old_it = old_submodules.begin();
           old_it != old_submodules.end();) {
        if (new_submodule->name == (*old_it)->name) {
          // Direct match. Add this module back to the submodule list, and
          // recurse on it
          Module *old_submodule = *old_it;
          m_old->submodules.emplace_back(old_submodule);
          update_module(old_submodule, new_submodule);
          // Remove from the old_submodules list so we don't delete it later
          old_it = old_submodules.erase(old_it);
          // Notify that we changed this node
          ItemChanged(wxDataViewItem(old_submodule));
          did_update_in_place = true;
          break;
        } else {
          ++old_it;
        }
      }

      // If we didn't update in place, we need to just add this node
      if (!did_update_in_place) {
        // Add it to the submodule list
        m_old->submodules.emplace_back(new_submodule);
        // Reparent the module
        new_submodule->parent = m_old;
        // Notify wx about it
        ItemAdded(wxDataViewItem(m_old), wxDataViewItem(new_submodule));
        // Detach it from the input design
        new_it = m_new->submodules.erase(new_it);
      } else {
        ++new_it;
      }
    }

    // At this point, we've handled cases 1 and 2. For the final case we
    // just need to take everything left in old_submodules that wasn't
    // readded and tell wx it's gone
    for (auto mod : old_submodules) {
      ItemDeleted(wxDataViewItem(m_old), wxDataViewItem(mod));
      delete_module_tree(mod);
    }
  };

  // Recursively update
  update_module(_design->top, d->top);
}

YostatDataModel::~YostatDataModel() { delete _design; }

YostatWxPanel::YostatWxPanel(std::string filename, Design *design)
    : wxFrame(nullptr, wxID_ANY, "Yostat", wxPoint(-1, -1), wxSize(-1, -1)),
      _filename(filename) {

  // Create a parent panel and sizer
  wxPanel *parent = new wxPanel(this, wxID_ANY);
  wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);

  // Create a dataview and add it to our sizer
  _dataview =
      new wxDataViewCtrl(parent, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1));
  hbox->Add(_dataview, -1, wxEXPAND);

  // Create our data model using the parsed yosys design
  _datamodel = new YostatDataModel(design);
  _dataview->AssociateModel(_datamodel);
  _datamodel->DecRef();

  // Generate display columns
  create_columns_for_design(design, /*sort*/ true);

  // Add a menu bar
  wxMenuBar *menubar = new wxMenuBar();
  wxMenu *menu = new wxMenu();
  menu->Append(Ids::RELOAD_FILE, "&Reload\tCTRL+R", "Reload current json file");
  menubar->Append(menu, "Yo&stat");
  SetMenuBar(menubar);

  // Status bar
  CreateStatusBar();
  GetStatusBar()->SetStatusText("Ready");

  // Finalize layout
  parent->SetSizer(hbox);
  parent->SetAutoLayout(true);
}

void YostatWxPanel::create_columns_for_design(Design *design, bool sort) {
  // Create the first column, which is the module names
  wxDataViewTextRenderer *string_renderer =
      new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_ACTIVATABLE);
  wxDataViewColumn *col_module =
      new wxDataViewColumn("Module Name", string_renderer, 0, 300, wxALIGN_LEFT,
                           wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
  _dataview->AppendColumn(col_module);

  // Create each of the primitive columns
  int col = 1;
  for (auto &cell : design->primitives) {
    wxDataViewTextRenderer *long_renderer =
        new wxDataViewTextRenderer("long", wxDATAVIEW_CELL_INERT);
    wxDataViewColumn *cell_col = new wxDataViewColumn(
        cell, long_renderer, col++, 100, wxALIGN_LEFT,
        wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE |
            wxDATAVIEW_COL_REORDERABLE);
    _dataview->AppendColumn(cell_col);
  }

  // Order by module name initially
  if (sort) {
    col_module->SetSortOrder(true);
    _datamodel->Resort();
  }
}

void YostatWxPanel::reload(wxCommandEvent &evt) {
  // Reread the json
  GetStatusBar()->SetStatusText("Re-reading " + _filename);
  Design *d = read_json(_filename);

  // Get the name of the column we were previously sorted by
  wxDataViewColumn *sort_col = _dataview->GetSortingColumn();
  const unsigned sort_col_idx = sort_col->GetModelColumn();
  const bool sorted_by_primitive = sort_col_idx > 0;
  const bool sort_order = sort_col->IsSortOrderAscending();
  std::string sort_primitive;
  if (sorted_by_primitive) {
    // Get the name of the sort primitive so we can re-sort by it
    sort_primitive = _datamodel->get_design()->primitives[sort_col_idx - 1];
  }

  // Clear the display columns
  _dataview->ClearColumns();

  // Create a model with the new data
  _datamodel->set_design(d);

  // Regenerate the dataview columns to match the new primitive data
  create_columns_for_design(d, /*sort*/ false);

  // Re-apply the sort if possible
  if (sorted_by_primitive) {
    // Try and match the primitive to a new column index
    for (unsigned i = 0; i < d->primitives.size(); i++) {
      if (sort_primitive == d->primitives[i]) {
        // Matched, sory by this colindex
        _dataview->GetColumn(i + 1)->SetSortOrder(sort_order);
        break;
      }
    }
  } else {
    _dataview->GetColumn(0)->SetSortOrder(sort_order);
  }
  _datamodel->Resort();

  GetStatusBar()->SetStatusText("Done");

  // Delete any parts of the new design that we didn't steal in set_design
  delete d;
}
