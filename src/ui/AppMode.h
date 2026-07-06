#pragma once

namespace OpenC3::UI {

/// Which top-level workflow the app is currently focused on.
/// PluginCreation: Home, Workspace (plugin/file/edit only) - no connection
/// required up front, connects transparently in the background.
/// ConnectOperate: Environment, Validator, Packet Tools, Logs, plus
/// Workspace's Check & Build & Install step - requires a real connection.
///
/// Kept in its own header (rather than nested in MainWindow.h) so it can be
/// referenced - e.g. by Application.cpp's startup-dialog decision - without
/// pulling in QMainWindow/Qt Widgets.
enum class AppMode { PluginCreation, ConnectOperate };

} // namespace OpenC3::UI
