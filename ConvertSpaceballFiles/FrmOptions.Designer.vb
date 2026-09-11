<Global.Microsoft.VisualBasic.CompilerServices.DesignerGenerated()> _
Partial Class FrmOptions
    Inherits System.Windows.Forms.Form

    'Form overrides dispose to clean up the component list.
    <System.Diagnostics.DebuggerNonUserCode()> _
    Protected Overrides Sub Dispose(ByVal disposing As Boolean)
        Try
            If disposing AndAlso components IsNot Nothing Then
                components.Dispose()
            End If
        Finally
            MyBase.Dispose(disposing)
        End Try
    End Sub

    'Required by the Windows Form Designer
    Private components As System.ComponentModel.IContainer

    'NOTE: The following procedure is required by the Windows Form Designer
    'It can be modified using the Windows Form Designer.  
    'Do not modify it using the code editor.
    <System.Diagnostics.DebuggerStepThrough()> _
    Private Sub InitializeComponent()
        Me.components = New System.ComponentModel.Container
        Me.tvDevices = New System.Windows.Forms.TreeView
        Me.btnDirectorySelect = New System.Windows.Forms.Button
        Me.btnAll = New System.Windows.Forms.Button
        Me.btnNone = New System.Windows.Forms.Button
        Me.btnTimerOn = New System.Windows.Forms.Button
        Me.btnTimerOff = New System.Windows.Forms.Button
        Me.btnIndexOff = New System.Windows.Forms.Button
        Me.btnIndexOn = New System.Windows.Forms.Button
        Me.btnProcess = New System.Windows.Forms.Button
        Me.btnCancel = New System.Windows.Forms.Button
        Me.StatusStrip = New System.Windows.Forms.StatusStrip
        Me.txtDirectory = New System.Windows.Forms.ToolStripStatusLabel
        Me.txtRootName = New System.Windows.Forms.ToolStripStatusLabel
        Me.txtLabel = New System.Windows.Forms.ToolStripStatusLabel
        Me.btnStop = New System.Windows.Forms.Button
        Me.Timer1 = New System.Windows.Forms.Timer(Me.components)
        Me.rbFITS = New System.Windows.Forms.RadioButton
        Me.rbCSV = New System.Windows.Forms.RadioButton
        Me.rbBoth = New System.Windows.Forms.RadioButton
        Me.StatusStrip.SuspendLayout()
        Me.SuspendLayout()
        '
        'tvDevices
        '
        Me.tvDevices.Anchor = CType((((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Bottom) _
                    Or System.Windows.Forms.AnchorStyles.Left) _
                    Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.tvDevices.CheckBoxes = True
        Me.tvDevices.Location = New System.Drawing.Point(12, 12)
        Me.tvDevices.Name = "tvDevices"
        Me.tvDevices.Size = New System.Drawing.Size(294, 425)
        Me.tvDevices.TabIndex = 0
        '
        'btnDirectorySelect
        '
        Me.btnDirectorySelect.Anchor = CType((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.btnDirectorySelect.Location = New System.Drawing.Point(312, 12)
        Me.btnDirectorySelect.Name = "btnDirectorySelect"
        Me.btnDirectorySelect.Size = New System.Drawing.Size(75, 23)
        Me.btnDirectorySelect.TabIndex = 1
        Me.btnDirectorySelect.Text = "Directory"
        Me.btnDirectorySelect.UseVisualStyleBackColor = True
        '
        'btnAll
        '
        Me.btnAll.Anchor = CType((System.Windows.Forms.AnchorStyles.Bottom Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.btnAll.Location = New System.Drawing.Point(312, 250)
        Me.btnAll.Name = "btnAll"
        Me.btnAll.Size = New System.Drawing.Size(75, 23)
        Me.btnAll.TabIndex = 2
        Me.btnAll.Text = "Select All"
        Me.btnAll.UseVisualStyleBackColor = True
        '
        'btnNone
        '
        Me.btnNone.Anchor = CType((System.Windows.Forms.AnchorStyles.Bottom Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.btnNone.Location = New System.Drawing.Point(312, 279)
        Me.btnNone.Name = "btnNone"
        Me.btnNone.Size = New System.Drawing.Size(75, 23)
        Me.btnNone.TabIndex = 3
        Me.btnNone.Text = "Select None"
        Me.btnNone.UseVisualStyleBackColor = True
        '
        'btnTimerOn
        '
        Me.btnTimerOn.Anchor = CType((System.Windows.Forms.AnchorStyles.Bottom Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.btnTimerOn.Location = New System.Drawing.Point(312, 327)
        Me.btnTimerOn.Name = "btnTimerOn"
        Me.btnTimerOn.Size = New System.Drawing.Size(75, 23)
        Me.btnTimerOn.TabIndex = 4
        Me.btnTimerOn.Text = "Timer On"
        Me.btnTimerOn.UseVisualStyleBackColor = True
        '
        'btnTimerOff
        '
        Me.btnTimerOff.Anchor = CType((System.Windows.Forms.AnchorStyles.Bottom Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.btnTimerOff.Location = New System.Drawing.Point(312, 356)
        Me.btnTimerOff.Name = "btnTimerOff"
        Me.btnTimerOff.Size = New System.Drawing.Size(75, 23)
        Me.btnTimerOff.TabIndex = 5
        Me.btnTimerOff.Text = "Timer Off"
        Me.btnTimerOff.UseVisualStyleBackColor = True
        '
        'btnIndexOff
        '
        Me.btnIndexOff.Anchor = CType((System.Windows.Forms.AnchorStyles.Bottom Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.btnIndexOff.Location = New System.Drawing.Point(312, 414)
        Me.btnIndexOff.Name = "btnIndexOff"
        Me.btnIndexOff.Size = New System.Drawing.Size(75, 23)
        Me.btnIndexOff.TabIndex = 7
        Me.btnIndexOff.Text = "Index Off"
        Me.btnIndexOff.UseVisualStyleBackColor = True
        '
        'btnIndexOn
        '
        Me.btnIndexOn.Anchor = CType((System.Windows.Forms.AnchorStyles.Bottom Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.btnIndexOn.Location = New System.Drawing.Point(312, 385)
        Me.btnIndexOn.Name = "btnIndexOn"
        Me.btnIndexOn.Size = New System.Drawing.Size(75, 23)
        Me.btnIndexOn.TabIndex = 6
        Me.btnIndexOn.Text = "Index On"
        Me.btnIndexOn.UseVisualStyleBackColor = True
        '
        'btnProcess
        '
        Me.btnProcess.Anchor = CType((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.btnProcess.Location = New System.Drawing.Point(312, 51)
        Me.btnProcess.Name = "btnProcess"
        Me.btnProcess.Size = New System.Drawing.Size(75, 23)
        Me.btnProcess.TabIndex = 8
        Me.btnProcess.Text = "Process"
        Me.btnProcess.UseVisualStyleBackColor = True
        '
        'btnCancel
        '
        Me.btnCancel.Anchor = CType((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.btnCancel.Location = New System.Drawing.Point(312, 109)
        Me.btnCancel.Name = "btnCancel"
        Me.btnCancel.Size = New System.Drawing.Size(75, 23)
        Me.btnCancel.TabIndex = 9
        Me.btnCancel.Text = "Exit"
        Me.btnCancel.UseVisualStyleBackColor = True
        '
        'StatusStrip
        '
        Me.StatusStrip.Items.AddRange(New System.Windows.Forms.ToolStripItem() {Me.txtDirectory, Me.txtRootName, Me.txtLabel})
        Me.StatusStrip.Location = New System.Drawing.Point(0, 456)
        Me.StatusStrip.Name = "StatusStrip"
        Me.StatusStrip.Size = New System.Drawing.Size(399, 22)
        Me.StatusStrip.TabIndex = 10
        Me.StatusStrip.Text = "StatusStrip1"
        '
        'txtDirectory
        '
        Me.txtDirectory.IsLink = True
        Me.txtDirectory.Name = "txtDirectory"
        Me.txtDirectory.Size = New System.Drawing.Size(55, 17)
        Me.txtDirectory.Text = "Directory"
        '
        'txtRootName
        '
        Me.txtRootName.IsLink = True
        Me.txtRootName.Name = "txtRootName"
        Me.txtRootName.Size = New System.Drawing.Size(61, 17)
        Me.txtRootName.Text = "rootName"
        '
        'txtLabel
        '
        Me.txtLabel.Name = "txtLabel"
        Me.txtLabel.Size = New System.Drawing.Size(55, 17)
        Me.txtLabel.Text = "Not Busy"
        '
        'btnStop
        '
        Me.btnStop.Anchor = CType((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.btnStop.Location = New System.Drawing.Point(312, 80)
        Me.btnStop.Name = "btnStop"
        Me.btnStop.Size = New System.Drawing.Size(75, 23)
        Me.btnStop.TabIndex = 11
        Me.btnStop.Text = "Stop"
        Me.btnStop.UseVisualStyleBackColor = True
        '
        'Timer1
        '
        '
        'rbFITS
        '
        Me.rbFITS.Anchor = CType((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.rbFITS.AutoSize = True
        Me.rbFITS.Checked = True
        Me.rbFITS.Location = New System.Drawing.Point(341, 156)
        Me.rbFITS.Name = "rbFITS"
        Me.rbFITS.Size = New System.Drawing.Size(48, 17)
        Me.rbFITS.TabIndex = 12
        Me.rbFITS.TabStop = True
        Me.rbFITS.Text = "FITS"
        Me.rbFITS.UseVisualStyleBackColor = True
        '
        'rbCSV
        '
        Me.rbCSV.Anchor = CType((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.rbCSV.AutoSize = True
        Me.rbCSV.Location = New System.Drawing.Point(341, 187)
        Me.rbCSV.Name = "rbCSV"
        Me.rbCSV.Size = New System.Drawing.Size(46, 17)
        Me.rbCSV.TabIndex = 13
        Me.rbCSV.Text = "CSV"
        Me.rbCSV.UseVisualStyleBackColor = True
        '
        'rbBoth
        '
        Me.rbBoth.Anchor = CType((System.Windows.Forms.AnchorStyles.Top Or System.Windows.Forms.AnchorStyles.Right), System.Windows.Forms.AnchorStyles)
        Me.rbBoth.AutoSize = True
        Me.rbBoth.Location = New System.Drawing.Point(341, 218)
        Me.rbBoth.Name = "rbBoth"
        Me.rbBoth.Size = New System.Drawing.Size(47, 17)
        Me.rbBoth.TabIndex = 14
        Me.rbBoth.Text = "Both"
        Me.rbBoth.UseVisualStyleBackColor = True
        '
        'FrmOptions
        '
        Me.AutoScaleDimensions = New System.Drawing.SizeF(6.0!, 13.0!)
        Me.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font
        Me.ClientSize = New System.Drawing.Size(399, 478)
        Me.Controls.Add(Me.rbBoth)
        Me.Controls.Add(Me.rbCSV)
        Me.Controls.Add(Me.rbFITS)
        Me.Controls.Add(Me.btnStop)
        Me.Controls.Add(Me.StatusStrip)
        Me.Controls.Add(Me.btnCancel)
        Me.Controls.Add(Me.btnProcess)
        Me.Controls.Add(Me.btnIndexOff)
        Me.Controls.Add(Me.btnIndexOn)
        Me.Controls.Add(Me.btnTimerOff)
        Me.Controls.Add(Me.btnTimerOn)
        Me.Controls.Add(Me.btnNone)
        Me.Controls.Add(Me.btnAll)
        Me.Controls.Add(Me.btnDirectorySelect)
        Me.Controls.Add(Me.tvDevices)
        Me.MinimumSize = New System.Drawing.Size(415, 514)
        Me.Name = "FrmOptions"
        Me.Text = "Spaceball Conversion Tool"
        Me.StatusStrip.ResumeLayout(False)
        Me.StatusStrip.PerformLayout()
        Me.ResumeLayout(False)
        Me.PerformLayout()

    End Sub
    Friend WithEvents tvDevices As System.Windows.Forms.TreeView
    Friend WithEvents btnDirectorySelect As System.Windows.Forms.Button
    Friend WithEvents btnAll As System.Windows.Forms.Button
    Friend WithEvents btnNone As System.Windows.Forms.Button
    Friend WithEvents btnTimerOn As System.Windows.Forms.Button
    Friend WithEvents btnTimerOff As System.Windows.Forms.Button
    Friend WithEvents btnIndexOff As System.Windows.Forms.Button
    Friend WithEvents btnIndexOn As System.Windows.Forms.Button
    Friend WithEvents btnProcess As System.Windows.Forms.Button
    Friend WithEvents btnCancel As System.Windows.Forms.Button
    Friend WithEvents StatusStrip As System.Windows.Forms.StatusStrip
    Friend WithEvents txtLabel As System.Windows.Forms.ToolStripStatusLabel
    Friend WithEvents btnStop As System.Windows.Forms.Button
    Friend WithEvents txtDirectory As System.Windows.Forms.ToolStripStatusLabel
    Friend WithEvents Timer1 As System.Windows.Forms.Timer
    Friend WithEvents rbFITS As System.Windows.Forms.RadioButton
    Friend WithEvents rbCSV As System.Windows.Forms.RadioButton
    Friend WithEvents rbBoth As System.Windows.Forms.RadioButton
    Friend WithEvents txtRootName As System.Windows.Forms.ToolStripStatusLabel

End Class
