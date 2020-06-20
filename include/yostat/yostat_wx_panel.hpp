#pragma once

#include <wx/dataview.h>
#include <wx/wx.h>

#include <yostat/parse.hpp>

class YostatWxPanel : public wxFrame {
public:
  YostatWxPanel(Design *d);
  DECLARE_EVENT_TABLE();

  void on_dataview_item_activated(wxDataViewEvent &evt);

private:
  Design *_design;
  wxDataViewCtrl *_dataview;
};
