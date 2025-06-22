# Internet Download Manager (C++ QT PROJ)

## Project Overview
This project is a robust Internet Download Manager application developed in C++ using the cross-platform Qt framework. It aims to provide users with a comprehensive tool for managing and accelerating their file downloads. The application features a modern graphical user interface (GUI) and integrates various functionalities to enhance the download experience, including detailed download information, customizable settings, and specialized dialogs for different download scenarios.

## Features
- **Efficient Download Management**: Supports concurrent downloads, allowing users to manage multiple files simultaneously. It provides progress tracking, pause/resume capabilities, and organized listing of all active and completed downloads.
- **Intuitive Graphical User Interface**: Built with Qt Widgets, the GUI offers a user-friendly experience with clear navigation and accessible controls for all download-related operations.
- **Customizable Connection Settings**: Users can configure network parameters such as proxy settings, maximum concurrent downloads, and retry attempts to optimize download performance based on their network environment.
- **Download Speed Limiter**: Allows users to set global or per-download speed limits, preventing the download manager from consuming all available bandwidth and ensuring a smooth browsing experience during downloads.
- **Detailed Download Information**: Provides comprehensive details for each download, including file size, progress, estimated time remaining, transfer rate, and source URL.
- **New Download Dialog**: A dedicated dialog for easily adding new downloads by pasting URLs, with options to specify save locations and file names.
- **Download Confirmation Dialog**: Confirms download initiation, providing a summary of the file to be downloaded before proceeding.
- **About Dialog**: Displays information about the application, such as version details and licensing.
- **YouTube Download Integration**: (Based on `youtubedownloaddialog.h`, `youtubedownloaddialog.cpp`) Suggests potential future or existing functionality for handling YouTube video downloads, possibly including format selection.

## Installation
To set up and run this project, ensure you have the following prerequisites installed:

### Prerequisites
- **Qt Framework**: Version 5.15 or newer is recommended. You can download it from the official Qt website.
- **CMake**: Version 3.14 or newer. Available from the CMake official website.
- **C++ Compiler**: A C++11 compatible compiler such as MinGW (on Windows), MSVC (Visual Studio on Windows), or GCC/Clang (on Linux/macOS).

### Building the Project
#### Using Qt Creator (Recommended)
1.  **Open Project**: Launch Qt Creator and select `File > Open File or Project...`. Navigate to the project root directory and select the `CMakeLists.txt` file.
2.  **Configure Project**: Qt Creator will prompt you to configure the project. Select your desired Qt kit (e.g., Desktop Qt 5.15.2 MinGW 64-bit).
3.  **Build**: Once configured, you can build the project by selecting `Build > Build Project "InternetDownloadManager"` or by pressing `Ctrl+B`.

#### Manual Build (Command Line)
1.  **Create Build Directory**: Open a terminal or command prompt and navigate to the project's root directory.
    ```bash
    mkdir build
    cd build
    ```
2.  **Configure with CMake**: Run CMake to generate build files. For Windows, you might specify a generator like `MinGW Makefiles` or `Visual Studio`.
    ```bash
    cmake ..
    # For Visual Studio (example):
    # cmake .. -G "Visual Studio 16 2019"
    ```
3.  **Build the Project**: Compile the project using the generated build system.
    ```bash
    cmake --build .
    ```
    Upon successful compilation, the executable `InternetDownloadManager.exe` (or similar, depending on your OS) will be located in the `build` directory or a subdirectory within it (e.g., `build/Debug` or `build/Release`).

## Usage
After successfully building the project, you can run the executable:

1.  **Locate Executable**: Navigate to the `build` directory (or its appropriate subdirectory, e.g., `build\Debug` or `build\Release`).
2.  **Run Application**: Double-click `InternetDownloadManager.exe` (on Windows) or execute it from the terminal.
    ```bash
    .\InternetDownloadManager.exe
    ```

### Key Interactions
-   **Main Window**: The central hub where all downloads are listed. You can view their status, progress, and manage them.
-   **Adding New Downloads**: Click the "New Download" button (or similar) to open the `NewDownloadDialog`. Paste your URL, choose a save location, and start the download.
-   **Adjusting Preferences**: Access the `PreferencesDialog` from the menu to modify general settings, connection parameters, and other application behaviors.
-   **Setting Speed Limits**: Use the `SpeedLimiterDialog` to control the maximum download speed, useful for managing network bandwidth.
-   **Viewing Download Details**: Select a download item to open the `DownloadDetailsDialog` for an in-depth look at its properties and progress.

## Project Structure
This project follows a modular structure, typical for Qt applications, separating UI definitions, header files, and source files.

-   `CMakeLists.txt`: The primary CMake build script that defines how the project is built, including source files, Qt modules, and executable targets.
-   `main.cpp`: The application's entry point, responsible for initializing the Qt application and displaying the main window.
-   **Main Window Components**:
    -   `mainwindow.h`, `mainwindow.cpp`, `mainwindow.ui`: Define the main application window, its layout, and core functionalities.
-   **Download Core Logic**:
    -   `downloadmanager.h`, `downloadmanager.cpp`: Implement the central logic for managing all downloads, including starting, pausing, resuming, and organizing them.
    -   `downloaditem.h`, `downloaditem.cpp`: Represent individual download tasks, encapsulating their state, progress, and specific download parameters.
-   **Dialogs and UI Components**:
    -   `newdownloaddialog.h`, `newdownloaddialog.cpp`, `newdownloaddialog.ui`: Handles the user interface and logic for adding new download URLs.
    -   `PreferencesDialog.h`, `PreferencesDialog.cpp`, `PreferencesDialog.ui`: Manages application-wide settings and user preferences.
    -   `aboutdialog.h`, `aboutdialog.cpp`, `about.ui`: Displays information about the application.
    -   `connectionsettingsdialog.h`, `connectionsettingsdialog.cpp`, `connectionsettingsdialog.ui`: Configures network connection parameters.
    -   `downloaddetailsdialog.h`, `downloaddetailsdialog.cpp`, `downloaddetailsdialog.ui`: Shows detailed information for a selected download.
    -   `downloadconfirmationdialog.h`, `downloadconfirmationdialog.cpp`, `downloadconfirmationdialog.ui`: Prompts the user for confirmation before starting a download.
    -   `speedlimiterdialog.h`, `speedlimiterdialog.cpp`, `speedlimiterdialog.ui`: Provides controls for setting download speed limits.
    -   `youtubedownloaddialog.h`, `youtubedownloaddialog.cpp`: (If implemented) Handles specific logic for YouTube video downloads.
-   **Utilities**:
    -   `utils.h`: Contains general utility functions used across the application.
-   **Build Artifacts**:
    -   `build/`: This directory is generated by CMake and contains all compiled binaries, intermediate files, and other build-related outputs.

```