chrome.contextMenus.onClicked.addListener((info, tab) => {
    if (info.menuItemId === "downloadLink" && info.linkUrl) {
        const encodedUrl = encodeURIComponent(info.linkUrl);
        console.log("Sending download request to:", `http://localhost:8080/download?url=${encodedUrl}`);
        fetch(`http://localhost:8080/download?url=${encodedUrl}`, {
            method: 'GET'
        })
        .then(response => response.text())
        .then(text => console.log("Server response:", text))
        .catch(error => console.error("Download request failed:", error));
    }
});

chrome.runtime.onInstalled.addListener(() => {
    chrome.contextMenus.create({
        id: "downloadLink",
        title: "Download with Manager",
        contexts: ["link"]
    });
});