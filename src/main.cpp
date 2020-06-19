#include <wx/cmdline.h>
#include <wx/wx.h>

#include <yostat/parse.hpp>
#include <yostat/yostat_wx_panel.hpp>

class YostatApp : public wxApp {
public:
  virtual bool OnInit() override;
  virtual void OnInitCmdLine(wxCmdLineParser &parser) override;
  virtual bool OnCmdLineParsed(wxCmdLineParser &parser) override;

private:
  YostatWxPanel *_panel = nullptr;
  std::string _json_file;
};

bool YostatApp::OnInit() {
  // Super
  if (!wxApp::OnInit())
    return false;

  // Attempt to parse the given file
  fprintf(stderr, "Parsing input from '%s'...\n", _json_file.c_str());
  Design *d = read_json(_json_file);
  if (!d) {
    fprintf(stderr, "Failed to parse input file '%s'\n", _json_file.c_str());
    return false;
  }

  // If we loaded it OK, display the data
  _panel = new YostatWxPanel(d);
  _panel->Show(true);
  return true;
}

void YostatApp::OnInitCmdLine(wxCmdLineParser &parser) {
  static const wxCmdLineEntryDesc cli_args[] = {
      {wxCMD_LINE_PARAM, nullptr, nullptr, "[json file]", wxCMD_LINE_VAL_STRING,
       wxCMD_LINE_OPTION_MANDATORY},
      {wxCMD_LINE_NONE, nullptr, nullptr, nullptr, wxCMD_LINE_VAL_NONE, 0}};

  parser.SetDesc(cli_args);
  parser.SetSwitchChars(wxT("-"));
}

bool YostatApp::OnCmdLineParsed(wxCmdLineParser &parser) {
  if (parser.GetParamCount() != 1) {
    fprintf(stderr, "Usage: yostat [json file]\n");
    return false;
  }
  _json_file = std::string(parser.GetParam(0));
  return true;
}

wxIMPLEMENT_APP(YostatApp);
