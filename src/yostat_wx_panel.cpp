#include <map>
#include <string>
#include <vector>

#include <wx/dataview.h>

#include <yostat/yostat_wx_panel.hpp>

class YostatDataModel : public wxDataViewModel {
public:
  YostatDataModel(Design *d) : _design(d) {}

  bool HasContainerColumns(const wxDataViewItem &item) const override {
    return true;
  }

  bool IsContainer(const wxDataViewItem &item) const override {
    Module *node = reinterpret_cast<Module *>(item.GetID());
    if (!node) {
      return true;
    }
    return node->submodules.size() > 0;
  }

  wxDataViewItem GetParent(const wxDataViewItem &item) const override {
    if (!item.IsOk()) {
      // Invisible root has no parent
      return wxDataViewItem(0);
    }

    Module *node = reinterpret_cast<Module *>(item.GetID());
    return wxDataViewItem((void *)node->parent);
  }

  unsigned int GetColumnCount() const override {
    return _design->primitives.size() + 1;
  }
  wxString GetColumnType(unsigned int col) const override {
    // return wxT("long");
    if (col == 0) {
      return wxT("string");
    }
    return wxT("long");
  }

  unsigned int GetChildren(const wxDataViewItem &item,
                           wxDataViewItemArray &children) const override {
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

  void GetValue(wxVariant &variant, const wxDataViewItem &item,
                unsigned int col) const {
    Module *node = reinterpret_cast<Module *>(item.GetID());
    if (col == 0) {
      variant = node->name;
      return;
    }

    variant = (long)node->get_primitive_count(_design->primitives[col - 1]);
  }

  bool SetValue(const wxVariant &variant, const wxDataViewItem &item,
                unsigned int col) {
    return false;
  }

  Design *_design;
};

YostatWxPanel::YostatWxPanel(Design *d)
    : wxFrame(nullptr, wxID_ANY, "Yostat", wxPoint(-1, -1), wxSize(-1, -1)),
      _design(d) {

  // Create a parent panel and sizer
  wxPanel *parent = new wxPanel(this, wxID_ANY);
  wxBoxSizer *hbox = new wxBoxSizer(wxHORIZONTAL);

  // Create a dataview and add it to our sizer
  wxDataViewCtrl *dataview =
      new wxDataViewCtrl(parent, wxID_ANY, wxPoint(-1, -1), wxSize(-1, -1));
  hbox->Add(dataview, -1, wxEXPAND);

  // Create our data model using the parsed yosys design
  wxDataViewModel *cells_model = new YostatDataModel(d);
  dataview->AssociateModel(cells_model);
  cells_model->DecRef();

  // Create the first column, which is the module names
  wxDataViewTextRenderer *string_renderer =
      new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
  wxDataViewColumn *col_module =
      new wxDataViewColumn("Module Name", string_renderer, 0, 300, wxALIGN_LEFT,
                           wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE);
  dataview->AppendColumn(col_module);

  // Create each of the primitive columns
  int col = 1;
  for (auto &cell : d->primitives) {
    wxDataViewTextRenderer *long_renderer =
        new wxDataViewTextRenderer("long", wxDATAVIEW_CELL_INERT);
    wxDataViewColumn *cell_col = new wxDataViewColumn(
        cell, long_renderer, col++, 100, wxALIGN_LEFT,
        wxDATAVIEW_COL_SORTABLE | wxDATAVIEW_COL_RESIZABLE |
            wxDATAVIEW_COL_REORDERABLE);
    dataview->AppendColumn(cell_col);
  }

  // Order by module name initially
  col_module->SetSortOrder(true);
  cells_model->Resort();

  // Finalize layout
  parent->SetSizer(hbox);
  parent->SetAutoLayout(true);
}
