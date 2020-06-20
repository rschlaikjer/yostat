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
  create_columns_for_design(design);

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

void YostatWxPanel::create_columns_for_design(Design *design) {
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
  col_module->SetSortOrder(true);
  _datamodel->Resort();
}

void YostatWxPanel::reload(wxCommandEvent &evt) {
  // Reread the json
  GetStatusBar()->SetStatusText("Re-reading " + _filename);
  Design *d = read_json(_filename);

  // Clear the display columns
  _dataview->ClearColumns();

  // Create a model with the new data
  _datamodel = new YostatDataModel(d);
  _dataview->AssociateModel(_datamodel);
  _datamodel->DecRef();

  // Regenerate the dataview columns to match the new primitive data
  create_columns_for_design(d);
  GetStatusBar()->SetStatusText("Done");
}
