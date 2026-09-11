Imports System.Runtime.InteropServices
Imports System.Threading

Module SpaceBallImports

    <DllImport("SpaceballConversion.dll", CharSet:=CharSet.Ansi)> _
       Function GetDirectoryInfo(ByVal directoryName As String, ByRef configText As Object) As IntPtr
    End Function

    <DllImport("SpaceballConversion.dll", CharSet:=CharSet.Ansi)> _
       Sub DeleteHandle(ByVal handle As IntPtr)
    End Sub

    <DllImport("SpaceballConversion.dll", CharSet:=CharSet.Ansi)> _
       Function SetConfigString(ByVal handle As IntPtr, ByVal configText As Object) As Int32
    End Function

    <DllImport("SpaceballConversion.dll", CharSet:=CharSet.Ansi)> _
       Sub ProcessDirectory(ByVal handle As IntPtr)
    End Sub

    <DllImport("SpaceballConversion.dll", CharSet:=CharSet.Ansi)> _
       Function ProcessDirectoryStart(ByVal handle As IntPtr) As IntPtr
    End Function

    <DllImport("SpaceballConversion.dll", CharSet:=CharSet.Ansi)> _
       Sub ProcessStop(ByVal handle As IntPtr)
    End Sub

    <DllImport("SpaceballConversion.dll", CharSet:=CharSet.Ansi)> _
       Function GetProcessStatus(ByVal handle As IntPtr, ByRef filename As Object, ByRef lastStatus As Object) As IntPtr
    End Function

    <DllImport("SpaceballConversion.dll", CharSet:=CharSet.Ansi)> _
       Function SetOutputType(ByVal handle As IntPtr, ByVal fits As Int32, ByVal csv As Int32) As Int32
    End Function

End Module

Public Class FrmOptions

    Private Class Device
        Public name As String
        Public guid As String
        Public deviceIndex As Integer
        Public channels As New List(Of String)

        Public selectedChannels As New List(Of String)
    End Class

    Private m_handle As IntPtr = Nothing
    Private m_process As IntPtr = Nothing
    Private m_thread As Thread = Nothing
    Private m_title As String = Nothing
    Private m_exiting As Boolean = False

    Private m_titleText As String = ""
    Private m_statusText As String = ""
    Private m_workingDirectory As String = ""



    ' Async calls
    Private setTitlePtr As New SetStringCallback(AddressOf SetTitle)
    Private setStatusPtr As New SetStringCallback(AddressOf SetStatus)
    Private activateUIPtr As New EnableFormCallback(AddressOf ActivateUI)
    Private setDirNameTextPtr As New SetStringCallback(AddressOf SetDirNameText)


    Private Delegate Sub PopulateTreeCallback(ByVal [devices] As List(Of Device))
    Private Delegate Sub EnableFormCallback(ByVal [enable] As Boolean)
    Private Delegate Sub SetStringCallback(ByVal [title] As String)


    ' Read the file
    ' Any line that starts with a semi-colon can be ignored entirely
    ' The data elements are embedded in brackets

    Private Function ConvertToFields(ByVal text As String) As List(Of String)
        Dim retv As New List(Of String)
        If (text Is Nothing) Then
            Return retv
        End If
        Dim lines As String() = text.Split(ControlChars.Lf)

        For Each line As String In lines
            If (Not line.Trim().StartsWith(";")) Then
                Dim delimString As String = "[]"
                Dim items() As String = line.Split(delimString.ToCharArray())
                For Each item As String In items
                    item = item.Trim
                    If (item.Length > 0) Then
                        retv.Add(item)
                    End If
                Next
            End If
        Next

        Return retv
    End Function

    Private Function ReadDevice(ByRef parameters As List(Of String), ByVal index As Integer, ByRef newDevice As Device) As Integer
        ' First three parameters are the deviceName, deviceGuid, deviceIndex, channelCount
        ' each device must present at least 1 channel (and, by default, will really present at least three)
        If index + 5 > parameters.Count Then
            Throw New SystemException("Malformed Config file - insufficient data for device")
        End If

        newDevice.name = parameters(index)
        newDevice.guid = parameters(index + 1)
        newDevice.deviceIndex = Conversion.Int(Conversion.Val(parameters(index + 2)))
        Dim channelCount As Integer = Conversion.Int(Conversion.Val(parameters(index + 3)))

        index += 4

        If index + channelCount > parameters.Count Then
            Throw New SystemException("Malformed Config File - channel data is missing")
        End If

        newDevice.channels = New List(Of String)

        For i As Integer = 1 To channelCount
            newDevice.channels.Add(parameters(index + i - 1))
        Next

        index += channelCount

        Return index
    End Function

    Private Function ParseDirectoryData(ByVal directoryData As String) As List(Of Device)
        Dim retv As New List(Of Device)
        Dim parameters As List(Of String) = ConvertToFields(directoryData)
        If (parameters.Count < 2) Then
            '            MsgBox("No devices found in " & filename)
            Return retv
        End If

        ' First parameter should be the count of devices
        Dim filename As String = parameters(0)
        Dim DevCount As Integer = Conversion.Int(Conversion.Val(parameters(1)))

        ' We need to keep reading devices as long as there is data
        ' start with the next field
        Dim index As Integer = 2

        Try
            While index < parameters.Count
                Dim newDevice As New Device
                index = ReadDevice(parameters, index, newDevice)
                retv.Add(newDevice)
            End While
        Catch ex As Exception
            MsgBox("Error in text returned from the DLL")
        End Try

        Return retv
    End Function

    Private Sub PopulateTree(ByVal devices As List(Of Device))
        tvDevices.Nodes.Clear()

        For Each source As Device In devices
            Dim node As TreeNode = tvDevices.Nodes.Add(source.name)
            For Each channel As String In source.channels
                node.Nodes.Add(channel).Checked = True
            Next
            node.Checked = True
            node.Tag = source
            node.Expand()
        Next
    End Sub

    Private Sub SelectDirectory(ByVal directory As String)
        Me.Invoke(activateUIPtr, New Object() {False})
        Dim variantText As Object = Nothing
        m_handle = SpaceBallImports.GetDirectoryInfo(directory, variantText)
        Dim d As New PopulateTreeCallback(AddressOf PopulateTree)
        Me.Invoke(d, New Object() {ParseDirectoryData(variantText)})
        Me.Invoke(activateUIPtr, New Object() {True})
        Me.Invoke(setDirNameTextPtr, New Object() {directory})
    End Sub

    Private Sub btnDirectorySelect_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnDirectorySelect.Click
        Dim folderBrowser As New FolderBrowserDialog

        folderBrowser.SelectedPath = My.Settings.LastDirectory
        If (folderBrowser.ShowDialog() = Windows.Forms.DialogResult.OK) Then
            Debug.Print(folderBrowser.SelectedPath)
            My.Settings.LastDirectory = folderBrowser.SelectedPath
            txtDirectory.Text = folderBrowser.SelectedPath
            ActivateUI(False)
            m_thread = New Thread(AddressOf SelectDirectory)
            m_thread.Start(folderBrowser.SelectedPath)
        End If

    End Sub

    Private Sub UpdateParentCheck(ByRef changedNode As TreeNode)
        If changedNode.Parent Is Nothing Then
            Exit Sub
        End If

        ' If we have a check, then the parent MUST be active
        If changedNode.Checked = True Then
            changedNode.Parent.Checked = True
            Exit Sub
        End If

        ' If all the nodes are unchecked, then uncheck the parent
        Dim checkParent As Boolean = False
        For Each childNode As TreeNode In changedNode.Parent.Nodes
            If childNode.Checked = True Then
                checkParent = True
                Exit For
            End If
        Next

        changedNode.Parent.Checked = checkParent

    End Sub

    ' Updates all child tree nodes recursively.
    Private Sub CheckAllChildNodes(ByVal treeNode As TreeNode, ByVal nodeChecked As Boolean)
        Dim node As TreeNode
        For Each node In treeNode.Nodes
            node.Checked = nodeChecked
            If node.Nodes.Count > 0 Then
                ' If the current node has child nodes, call the CheckAllChildsNodes method recursively.
                Me.CheckAllChildNodes(node, nodeChecked)
            End If
        Next node
    End Sub

    Private Sub tvDevices_AfterCheck(ByVal sender As Object, ByVal e As System.Windows.Forms.TreeViewEventArgs) Handles tvDevices.AfterCheck
        If e.Action <> TreeViewAction.Unknown Then
            If e.Node.Nodes.Count > 0 Then
                ' Calls the CheckAllChildNodes method, passing in the current 
                ' Checked value of the TreeNode whose checked state changed. 
                Me.CheckAllChildNodes(e.Node, e.Node.Checked)
            End If

            UpdateParentCheck(e.Node)

            ' if we are selecting a child node, then the parent must be selected
            If e.Node.Checked AndAlso e.Node.Parent IsNot Nothing Then
                e.Node.Parent.Checked = True
            End If

        End If
    End Sub

    Private Sub btnAll_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnAll.Click
        For Each node As TreeNode In tvDevices.Nodes
            node.Checked = True
            CheckAllChildNodes(node, node.Checked)
        Next
    End Sub

    Private Sub btnNone_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnNone.Click
        For Each node As TreeNode In tvDevices.Nodes
            node.Checked = False
            CheckAllChildNodes(node, node.Checked)
        Next
    End Sub

    Private Sub SetChannelNameChecked(ByVal name As String, ByVal checked As Boolean)
        For Each root As TreeNode In tvDevices.Nodes
            For Each node As TreeNode In root.Nodes
                If node.Text = name Then
                    node.Checked = checked
                    UpdateParentCheck(node)
                    Exit For
                End If
            Next
        Next
    End Sub

    Private Sub btnTimerOn_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnTimerOn.Click
        SetChannelNameChecked("implicitTimer", True)
    End Sub

    Private Sub btnTimerOff_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnTimerOff.Click
        SetChannelNameChecked("implicitTimer", False)
    End Sub

    Private Sub btnIndexOn_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnIndexOn.Click
        SetChannelNameChecked("index", True)
    End Sub

    Private Sub btnIndexOff_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnIndexOff.Click
        SetChannelNameChecked("index", False)
    End Sub

    Private Sub btnCancel_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnCancel.Click
        Close()
    End Sub

    Private Sub AddOutputParameter(ByRef outputData As List(Of String), ByVal parameter As Object)
        outputData.Add("[" & parameter.ToString & "]")
    End Sub

    Private Sub SetTitle(ByVal title As String)
        Me.Text = title
    End Sub

    Private Sub SetStatus(ByVal text As String)
        Me.txtLabel.Text = text
    End Sub

    Private Sub SetDirNameText(ByVal directory As String)
        Me.txtDirectory.Text = GetLastDirNode(directory)
    End Sub

    Private Sub ProcessFiles(ByVal conversionText As String)
        Dim failureCode As Integer = SpaceBallImports.SetConfigString(m_handle, conversionText)
        If failureCode <> 0 Then
            MsgBox("Unable to set this configuration" & ControlChars.CrLf & "Abandoning Processing")
            Me.Invoke(activateUIPtr, New Object() {True})
            Exit Sub
        End If

        My.Settings.LastConfig = conversionText

        m_process = SpaceBallImports.ProcessDirectoryStart(m_handle)

        While m_process <> 0
            Dim filename As String = Nothing
            Dim status As String = Nothing
            Thread.Sleep(100)
            m_process = SpaceBallImports.GetProcessStatus(m_process, filename, status)
        End While

        'Me.Invoke(activateUIPtr, New Object() {True})
        'Me.Invoke(setStatusPtr, New Object() {""})
    End Sub

    Private Sub ActivateUI(ByVal isOn As Boolean)
        If Not isOn Then
            Me.Text = m_title + " - Working"
        Else
            Me.Text = m_title
        End If

        For Each Control As Control In Me.Controls
            Control.Enabled = isOn
        Next
        ' txtLabel.Enabled = True
        ' txtDirectory.Enabled = True
        StatusStrip.Enabled = True
        btnStop.Enabled = True
    End Sub

    Private Sub btnProcess_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnProcess.Click
        Dim outputDevices As New List(Of Device)
        For Each node As TreeNode In tvDevices.Nodes
            If node.Checked Then
                Dim d As Device = node.Tag
                d.selectedChannels.Clear()
                Debug.Print(d.name)
                For Each channel As TreeNode In node.Nodes
                    If channel.Checked Then
                        d.selectedChannels.Add(channel.Text)
                        Debug.Print("    " & channel.Text)
                    End If
                Next

                outputDevices.Add(d)
            End If
        Next

        If outputDevices.Count() < 1 Then
            MsgBox("No output devices selected, Click Cancel, not Okay")
            Exit Sub
        End If

        ' create the output data
        Dim outputData As New List(Of String)
        outputData.Add("; Found " & outputDevices.Count() & " devices" & ControlChars.CrLf)
        outputData.Add("[config]" & ControlChars.CrLf)
        AddOutputParameter(outputData, outputDevices.Count)
        For Each dev As Device In outputDevices
            AddOutputParameter(outputData, dev.name)
            AddOutputParameter(outputData, dev.guid)
            AddOutputParameter(outputData, dev.deviceIndex)
            AddOutputParameter(outputData, dev.selectedChannels.Count())
            outputData.Add(ControlChars.CrLf)
            For Each channel As String In dev.selectedChannels
                AddOutputParameter(outputData, channel)
            Next
            outputData.Add(ControlChars.CrLf)
        Next

        Dim conversionText As String = ""
        For Each line As String In outputData
            conversionText += line + ControlChars.CrLf
        Next

        SetOutputType(m_handle, rbBoth.Checked OrElse rbFITS.Checked, rbBoth.Checked OrElse rbCSV.Checked)

        m_thread = New Thread(AddressOf ProcessFiles)
        ActivateUI(False)
        m_thread.Start(conversionText)
        Timer1.Start()

    End Sub

    Private Sub FrmOptions_FormClosed(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosedEventArgs) Handles Me.FormClosed
        If m_thread IsNot Nothing Then
            m_thread.Join()
        End If
    End Sub

    Private Sub FrmOptions_FormClosing(ByVal sender As Object, ByVal e As System.Windows.Forms.FormClosingEventArgs) Handles Me.FormClosing
        My.Settings.LastSize = Me.Size
        If (m_process <> 0 AndAlso m_thread IsNot Nothing) Then

            m_exiting = True
            SpaceBallImports.ProcessStop(Me.m_process)

            'm_thread.Join()
        End If
    End Sub

    Private Sub FrmOptions_Load(ByVal sender As Object, ByVal e As System.EventArgs) Handles Me.Load
        m_title = Me.Text
        CancelButton = btnCancel
        m_workingDirectory = My.Settings.LastDirectory
        txtDirectory.Text = GetLastDirNode(m_workingDirectory)
        m_thread = New Thread(AddressOf SelectDirectory)
        m_thread.Start(My.Settings.LastDirectory)

        Me.Size = My.Settings.LastSize
        Timer1.Interval = 30
        Timer1.Stop()
    End Sub

    Private Sub btnStop_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles btnStop.Click
        SpaceBallImports.ProcessStop(Me.m_process)
    End Sub

    Private Function GetLastDirNode(ByVal filename As String) As String
        If filename.Contains("\") Then
            Dim offset As Integer = filename.LastIndexOf("\")
            filename = filename.Substring(offset + 1)
        End If
        Return filename
    End Function

    Private Function GetLastDir(ByVal filename As String) As String
        If filename.Contains("\") Then
            Dim offset As Integer = filename.LastIndexOf("\")
            filename = filename.Substring(0, offset)
            Return GetLastDirNode(filename)
        End If
        Return filename
    End Function

    Private Function GetDirectory(ByVal filename As String) As String
        If filename.Contains("\") Then
            Dim offset As Integer = filename.LastIndexOf("\")
            filename = filename.Substring(0, offset)
        End If
        Return filename
    End Function

    Dim lastFilename As String = ""
    Dim lastStatus As String = ""
    Private Sub Timer1_Tick(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles Timer1.Tick
        If m_process = 0 Then
            Timer1.Stop()
            ActivateUI(True)
            SetStatus("")
            m_workingDirectory = My.Settings.LastDirectory
            txtDirectory.Text = GetLastDirNode(m_workingDirectory)
        End If

        Dim filename As String = Nothing
        Dim status As String = Nothing
        m_process = SpaceBallImports.GetProcessStatus(m_process, filename, status)

        If m_process = 0 Then
            Exit Sub
        End If

        m_workingDirectory = GetDirectory(filename)
        Dim directory As String = GetLastDir(filename)
        txtDirectory.Text = directory

        filename = GetLastDirNode(filename)

        If m_exiting Then
            ' We are done, do not do anything more!
            Exit Sub
        End If

        If filename IsNot Nothing AndAlso filename.Length > 0 AndAlso filename <> lastFilename Then
            Me.Invoke(setTitlePtr, New Object() {"Processing " & filename})
            lastFilename = filename
        End If

        If status IsNot Nothing AndAlso status.Length > 0 AndAlso status <> lastStatus Then
            status = status.Trim
            Me.Invoke(setStatusPtr, New Object() {status})
            lastStatus = status
        End If
    End Sub

    Private Sub txtDirectory_Click(ByVal sender As System.Object, ByVal e As System.EventArgs) Handles txtDirectory.Click
        ' Launch the directory in explorer
        Dim folder As New Process
        folder.StartInfo.UseShellExecute = True
        folder.StartInfo.FileName = m_workingDirectory
        folder.Start()
    End Sub

End Class
