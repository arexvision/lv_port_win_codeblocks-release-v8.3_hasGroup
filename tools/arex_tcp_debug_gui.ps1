Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

[System.Windows.Forms.Application]::EnableVisualStyles()

$script:Client = $null
$script:Stream = $null
$script:RxBuffer = New-Object byte[] 4096

function Append-Log {
    param([string]$Text)

    $logBox.AppendText($Text)
    $logBox.SelectionStart = $logBox.TextLength
    $logBox.ScrollToCaret()
}

function Set-Connected {
    param([bool]$Connected)

    $connectButton.Enabled = -not $Connected
    $disconnectButton.Enabled = $Connected
    $sendButton.Enabled = $Connected
    $statusLabel.Text = if ($Connected) { "Connected" } else { "Disconnected" }
}

function Disconnect-Tcp {
    if ($script:Stream -ne $null) {
        try { $script:Stream.Close() } catch {}
        $script:Stream = $null
    }

    if ($script:Client -ne $null) {
        try { $script:Client.Close() } catch {}
        $script:Client = $null
    }

    Set-Connected $false
}

function Connect-Tcp {
    if ($script:Client -ne $null) {
        return
    }

    $hostText = $hostBox.Text.Trim()
    $port = 0
    if (-not [int]::TryParse($portBox.Text.Trim(), [ref]$port)) {
        [System.Windows.Forms.MessageBox]::Show("Port must be a number.", "Bad port") | Out-Null
        return
    }

    try {
        $client = New-Object System.Net.Sockets.TcpClient
        $connect = $client.BeginConnect($hostText, $port, $null, $null)
        if (-not $connect.AsyncWaitHandle.WaitOne(3000)) {
            $client.Close()
            throw "Connect timeout"
        }

        $client.EndConnect($connect)
        $script:Client = $client
        $script:Stream = $client.GetStream()
        Set-Connected $true
        Append-Log "[APP] Connected to ${hostText}:${port}`r`n"
    } catch {
        Disconnect-Tcp
        Append-Log "[APP] Connect failed: $($_.Exception.Message)`r`n"
        Append-Log "[APP] Please start the simulator first, then connect again.`r`n"
    }
}

function Send-Line {
    param([string]$Text)

    if ($script:Stream -eq $null) {
        Append-Log "[APP] Not connected.`r`n"
        return
    }

    if ([string]::IsNullOrWhiteSpace($Text)) {
        return
    }

    if (-not $Text.EndsWith("`n") -and -not $Text.EndsWith("`r")) {
        $Text = "$Text`r`n"
    }

    try {
        $bytes = [System.Text.Encoding]::UTF8.GetBytes($Text)
        $script:Stream.Write($bytes, 0, $bytes.Length)
        Append-Log "> $Text"
    } catch {
        Append-Log "[APP] Send failed: $($_.Exception.Message)`r`n"
        Disconnect-Tcp
    }
}

$form = New-Object System.Windows.Forms.Form
$form.Text = "AREX TCP Debug"
$form.StartPosition = "CenterScreen"
$form.MinimumSize = New-Object System.Drawing.Size(640, 400)
$form.Size = New-Object System.Drawing.Size(780, 520)

$topPanel = New-Object System.Windows.Forms.Panel
$topPanel.Dock = "Top"
$topPanel.Height = 42
$topPanel.Padding = New-Object System.Windows.Forms.Padding(8, 8, 8, 4)
$form.Controls.Add($topPanel)

$hostLabel = New-Object System.Windows.Forms.Label
$hostLabel.Text = "Host"
$hostLabel.Location = New-Object System.Drawing.Point(8, 12)
$hostLabel.AutoSize = $true
$topPanel.Controls.Add($hostLabel)

$hostBox = New-Object System.Windows.Forms.TextBox
$hostBox.Text = "127.0.0.1"
$hostBox.Location = New-Object System.Drawing.Point(48, 8)
$hostBox.Width = 140
$topPanel.Controls.Add($hostBox)

$portLabel = New-Object System.Windows.Forms.Label
$portLabel.Text = "Port"
$portLabel.Location = New-Object System.Drawing.Point(200, 12)
$portLabel.AutoSize = $true
$topPanel.Controls.Add($portLabel)

$portBox = New-Object System.Windows.Forms.TextBox
$portBox.Text = "7623"
$portBox.Location = New-Object System.Drawing.Point(238, 8)
$portBox.Width = 62
$topPanel.Controls.Add($portBox)

$connectButton = New-Object System.Windows.Forms.Button
$connectButton.Text = "Connect"
$connectButton.Location = New-Object System.Drawing.Point(314, 7)
$connectButton.Width = 86
$connectButton.Add_Click({ Connect-Tcp })
$topPanel.Controls.Add($connectButton)

$disconnectButton = New-Object System.Windows.Forms.Button
$disconnectButton.Text = "Disconnect"
$disconnectButton.Location = New-Object System.Drawing.Point(408, 7)
$disconnectButton.Width = 96
$disconnectButton.Enabled = $false
$disconnectButton.Add_Click({ Disconnect-Tcp })
$topPanel.Controls.Add($disconnectButton)

$statusLabel = New-Object System.Windows.Forms.Label
$statusLabel.Text = "Disconnected"
$statusLabel.Location = New-Object System.Drawing.Point(520, 12)
$statusLabel.AutoSize = $true
$topPanel.Controls.Add($statusLabel)

$sendPanel = New-Object System.Windows.Forms.Panel
$sendPanel.Dock = "Bottom"
$sendPanel.Height = 42
$sendPanel.Padding = New-Object System.Windows.Forms.Padding(8, 4, 8, 8)
$form.Controls.Add($sendPanel)

$sendButton = New-Object System.Windows.Forms.Button
$sendButton.Text = "Send"
$sendButton.Dock = "Right"
$sendButton.Width = 86
$sendButton.Enabled = $false
$sendButton.Add_Click({ Send-Line $inputBox.Text })
$sendPanel.Controls.Add($sendButton)

$inputBox = New-Object System.Windows.Forms.TextBox
$inputBox.Text = "state"
$inputBox.Dock = "Fill"
$inputBox.Add_KeyDown({
    if ($_.KeyCode -eq [System.Windows.Forms.Keys]::Enter) {
        Send-Line $inputBox.Text
        $_.SuppressKeyPress = $true
    }
})
$sendPanel.Controls.Add($inputBox)

$quickPanel = New-Object System.Windows.Forms.Panel
$quickPanel.Dock = "Bottom"
$quickPanel.Height = 38
$quickPanel.Padding = New-Object System.Windows.Forms.Padding(8, 2, 8, 4)
$form.Controls.Add($quickPanel)

$quickX = 8
foreach ($command in @("help", "state", "12.3", "depth 0", "manual on", "auto on")) {
    $button = New-Object System.Windows.Forms.Button
    $button.Text = $command
    $button.Tag = $command
    $button.Location = New-Object System.Drawing.Point($quickX, 4)
    $button.Width = 86
    $button.Add_Click({
        $inputBox.Text = [string]$this.Tag
        Send-Line $inputBox.Text
    })
    $quickPanel.Controls.Add($button)
    $quickX += 92
}

$logBox = New-Object System.Windows.Forms.TextBox
$logBox.Dock = "Fill"
$logBox.Multiline = $true
$logBox.ScrollBars = "Vertical"
$logBox.ReadOnly = $true
$logBox.WordWrap = $false
$logBox.Font = New-Object System.Drawing.Font("Consolas", 10)
$form.Controls.Add($logBox)

$timer = New-Object System.Windows.Forms.Timer
$timer.Interval = 50
$timer.Add_Tick({
    if ($script:Stream -eq $null -or $script:Client -eq $null) {
        return
    }

    try {
        while ($script:Stream.DataAvailable) {
            $count = $script:Stream.Read($script:RxBuffer, 0, $script:RxBuffer.Length)
            if ($count -le 0) {
                Append-Log "[APP] Remote closed connection.`r`n"
                Disconnect-Tcp
                return
            }

            $text = [System.Text.Encoding]::UTF8.GetString($script:RxBuffer, 0, $count)
            Append-Log $text
        }
    } catch {
        Append-Log "[APP] Receive failed: $($_.Exception.Message)`r`n"
        Disconnect-Tcp
    }
})
$timer.Start()

$form.Add_FormClosing({
    $timer.Stop()
    Disconnect-Tcp
})

[void]$form.ShowDialog()
