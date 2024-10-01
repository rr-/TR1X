﻿using System.Windows;
using TRX_ConfigToolLib;

namespace TR2X_ConfigTool;

public partial class App : Application
{
    public App()
    {
        Current.MainWindow = new TRXConfigWindow();
        Current.MainWindow.Show();
    }
}
