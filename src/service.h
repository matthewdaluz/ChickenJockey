#pragma once

// Starts the Chicken Jockey Windows Service.
// This function is called by main.cpp when not running in debug mode.
bool StartServiceHandler();
bool InstallService();
