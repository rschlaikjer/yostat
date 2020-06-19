#pragma once

#include <wx/wx.h>

#include <yostat/parse.hpp>

class YostatWxPanel : public wxFrame {
public:
  YostatWxPanel(Design *d);

private:
  Design *_design;
};
