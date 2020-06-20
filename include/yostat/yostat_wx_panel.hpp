#pragma once

#include <wx/dataview.h>
#include <wx/wx.h>

#include <yostat/parse.hpp>

class YostatDataModel : public wxDataViewModel {
public:
  YostatDataModel(Design *d) : _design(d) {}
  ~YostatDataModel();

  /* wxDataViewModel overrides */
  bool HasContainerColumns(const wxDataViewItem &item) const override;
  bool IsContainer(const wxDataViewItem &item) const override;
  wxDataViewItem GetParent(const wxDataViewItem &item) const override;
  unsigned int GetColumnCount() const override;
  wxString GetColumnType(unsigned int col) const override;
  unsigned int GetChildren(const wxDataViewItem &item,
                           wxDataViewItemArray &children) const override;
  void GetValue(wxVariant &variant, const wxDataViewItem &item,
                unsigned int col) const;
  bool SetValue(const wxVariant &variant, const wxDataViewItem &item,
                unsigned int col);

  void set_design(Design *d);
  Design *get_design();

private:
  Design *_design;
};

class YostatWxPanel : public wxFrame {
public:
  YostatWxPanel(std::string filename, Design *d);
  DECLARE_EVENT_TABLE();

  void create_columns_for_design(Design *design, bool sort);
  void on_dataview_item_activated(wxDataViewEvent &evt);
  void reload(wxCommandEvent &evt);

private:
  const std::string _filename;
  wxDataViewCtrl *_dataview;
  YostatDataModel *_datamodel;
};
