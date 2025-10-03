#ifndef HEADER_95200508464B4360923B8FC98607A039 // -*- mode:c++ -*-
#define HEADER_95200508464B4360923B8FC98607A039

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#include <vector>
#include <string>
#include <functional>

// Forward declarations
class OpenFileDialog;
class SaveFileDialog;

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

void MessageBox(const std::string &title, const std::string &text);

//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////


// std::function version for the interface
void SaveFileDialogGTKAsync(const std::vector<OpenFileDialog::Filter> &filters,
                           const std::string &default_path,
                           std::function<void(const std::string&)> callback);


void OpenFileDialogGTKAsync(const std::vector<OpenFileDialog::Filter> &filters,
                           const std::string &default_path,
                           std::function<void(const std::string&)> callback);

void SelectFolderDialogGTKAsync(const std::string &default_path,
                               void (*callback)(const std::string& path));

// Function to process GTK events (to be called from main SDL loop)
void ProcessGTKEvents();


//////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////

#endif
