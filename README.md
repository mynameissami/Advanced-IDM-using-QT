# Advanced Internet Download Manager

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
    cmake ..
    ```

3.  **Build the project:**

    ```bash
    cmake --build .
    ```

## Browser Extension Setup

To use the browser extension, follow these steps:

### For Chrome/Brave/Edge (Chromium-based browsers):

1.  Open your browser and navigate to `chrome://extensions` (or `brave://extensions`, `edge://extensions`).
2.  Enable "Developer mode" by toggling the switch in the top right corner.
3.  Click on "Load unpacked" and select the `InternetDownloadManagerExtension` folder located in the root of this project.

### For Firefox:

1.  Open Firefox and navigate to `about:debugging#/runtime/this-firefox`.
2.  Click on "Load Temporary Add-on..." and select any file inside the `InternetDownloadManagerExtension` folder (e.g., `manifest.json`).

Once installed, the extension will attempt to communicate with the running Internet Download Manager application.
