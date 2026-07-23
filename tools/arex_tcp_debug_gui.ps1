$source = @'
using System;
using System.Drawing;
using System.Net.Sockets;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Windows.Forms;

public sealed class ArexTcpDebugForm : Form
{
    private readonly TextBox hostBox = new TextBox();
    private readonly TextBox portBox = new TextBox();
    private readonly Button connectButton = new Button();
    private readonly Button disconnectButton = new Button();
    private readonly Label statusLabel = new Label();
    private readonly TextBox logBox = new TextBox();
    private readonly TextBox inputBox = new TextBox();
    private readonly Button sendButton = new Button();

    private TcpClient client;
    private NetworkStream stream;
    private CancellationTokenSource receiveCancel;

    public ArexTcpDebugForm()
    {
        Text = "AREX TCP Debug";
        StartPosition = FormStartPosition.CenterScreen;
        MinimumSize = new Size(640, 400);
        Size = new Size(780, 520);

        Panel topPanel = new Panel();
        topPanel.Dock = DockStyle.Top;
        topPanel.Height = 42;
        topPanel.Padding = new Padding(8, 8, 8, 4);
        Controls.Add(topPanel);

        Label hostLabel = new Label();
        hostLabel.Text = "Host";
        hostLabel.Location = new Point(8, 12);
        hostLabel.AutoSize = true;
        topPanel.Controls.Add(hostLabel);

        hostBox.Text = "127.0.0.1";
        hostBox.Location = new Point(48, 8);
        hostBox.Width = 140;
        topPanel.Controls.Add(hostBox);

        Label portLabel = new Label();
        portLabel.Text = "Port";
        portLabel.Location = new Point(200, 12);
        portLabel.AutoSize = true;
        topPanel.Controls.Add(portLabel);

        portBox.Text = "7623";
        portBox.Location = new Point(238, 8);
        portBox.Width = 62;
        topPanel.Controls.Add(portBox);

        connectButton.Text = "Connect";
        connectButton.Location = new Point(314, 7);
        connectButton.Width = 86;
        connectButton.Click += async delegate { await ConnectTcpAsync(); };
        topPanel.Controls.Add(connectButton);

        disconnectButton.Text = "Disconnect";
        disconnectButton.Location = new Point(408, 7);
        disconnectButton.Width = 96;
        disconnectButton.Enabled = false;
        disconnectButton.Click += delegate { DisconnectTcp(); };
        topPanel.Controls.Add(disconnectButton);

        statusLabel.Text = "Disconnected";
        statusLabel.Location = new Point(520, 12);
        statusLabel.AutoSize = true;
        topPanel.Controls.Add(statusLabel);

        Panel sendPanel = new Panel();
        sendPanel.Dock = DockStyle.Bottom;
        sendPanel.Height = 42;
        sendPanel.Padding = new Padding(8, 4, 8, 8);
        Controls.Add(sendPanel);

        sendButton.Text = "Send";
        sendButton.Dock = DockStyle.Right;
        sendButton.Width = 86;
        sendButton.Enabled = false;
        sendButton.Click += async delegate { await SendLineAsync(inputBox.Text); };
        sendPanel.Controls.Add(sendButton);

        inputBox.Text = "state";
        inputBox.Dock = DockStyle.Fill;
        inputBox.KeyDown += async delegate(object sender, KeyEventArgs e) {
            if (e.KeyCode == Keys.Enter)
            {
                e.SuppressKeyPress = true;
                await SendLineAsync(inputBox.Text);
            }
        };
        sendPanel.Controls.Add(inputBox);

        Panel quickPanel = new Panel();
        quickPanel.Dock = DockStyle.Bottom;
        quickPanel.Height = 76;
        quickPanel.Padding = new Padding(8, 2, 8, 4);
        Controls.Add(quickPanel);

        int quickX = 8;
        int quickY = 4;
        const int quickButtonWidth = 96;
        string[] commands = new string[] {
            "help", "state", "batt 15", "batt auto", "temp 8", "temp auto",
            "bat_temp 45", "prj_temp 50", "12.3", "depth 0", "manual on", "auto on"
        };
        foreach (string command in commands)
        {
            if (quickX + quickButtonWidth > 720)
            {
                quickX = 8;
                quickY += 34;
            }
            Button button = new Button();
            button.Text = command;
            button.Tag = command;
            button.Location = new Point(quickX, quickY);
            button.Width = quickButtonWidth;
            button.Click += async delegate(object sender, EventArgs e) {
                string text = (string)((Button)sender).Tag;
                inputBox.Text = text;
                await SendLineAsync(text);
            };
            quickPanel.Controls.Add(button);
            quickX += quickButtonWidth + 6;
        }

        logBox.Dock = DockStyle.Fill;
        logBox.Multiline = true;
        logBox.ScrollBars = ScrollBars.Vertical;
        logBox.ReadOnly = true;
        logBox.WordWrap = false;
        logBox.Font = new Font("Consolas", 10);
        Controls.Add(logBox);
    }

    protected override void OnFormClosing(FormClosingEventArgs e)
    {
        DisconnectTcp();
        base.OnFormClosing(e);
    }

    private async Task ConnectTcpAsync()
    {
        if (client != null)
        {
            return;
        }

        int port;
        if (!int.TryParse(portBox.Text.Trim(), out port))
        {
            MessageBox.Show("Port must be a number.", "Bad port");
            return;
        }

        string host = hostBox.Text.Trim();
        TcpClient pending = new TcpClient();

        try
        {
            Task connectTask = pending.ConnectAsync(host, port);
            Task timeoutTask = Task.Delay(3000);
            Task finished = await Task.WhenAny(connectTask, timeoutTask);
            if (finished != connectTask)
            {
                pending.Close();
                throw new TimeoutException("Connect timeout");
            }

            await connectTask;

            client = pending;
            stream = client.GetStream();
            receiveCancel = new CancellationTokenSource();
            SetConnected(true);
            AppendLog("[APP] Connected to " + host + ":" + port.ToString() + "\r\n");
            Task readTask = ReceiveLoopAsync(receiveCancel.Token);
        }
        catch (Exception ex)
        {
            pending.Close();
            DisconnectTcp();
            AppendLog("[APP] Connect failed: " + ex.Message + "\r\n");
            AppendLog("[APP] Please rebuild/start the simulator first, then connect again.\r\n");
        }
    }

    private async Task ReceiveLoopAsync(CancellationToken token)
    {
        byte[] buffer = new byte[4096];

        try
        {
            while (!token.IsCancellationRequested && stream != null)
            {
                int count = await stream.ReadAsync(buffer, 0, buffer.Length, token);
                if (count <= 0)
                {
                    AppendLog("[APP] Remote closed connection.\r\n");
                    break;
                }

                string text = Encoding.UTF8.GetString(buffer, 0, count);
                AppendLog(text);
            }
        }
        catch (ObjectDisposedException)
        {
        }
        catch (Exception ex)
        {
            if (!token.IsCancellationRequested)
            {
                AppendLog("[APP] Receive failed: " + ex.Message + "\r\n");
            }
        }

        if (!token.IsCancellationRequested)
        {
            BeginInvoke((Action)delegate { DisconnectTcp(); });
        }
    }

    private async Task SendLineAsync(string text)
    {
        if (stream == null)
        {
            AppendLog("[APP] Not connected.\r\n");
            return;
        }

        if (string.IsNullOrWhiteSpace(text))
        {
            return;
        }

        if (!text.EndsWith("\n") && !text.EndsWith("\r"))
        {
            text += "\r\n";
        }

        try
        {
            byte[] bytes = Encoding.UTF8.GetBytes(text);
            await stream.WriteAsync(bytes, 0, bytes.Length);
            AppendLog("> " + text);
        }
        catch (Exception ex)
        {
            AppendLog("[APP] Send failed: " + ex.Message + "\r\n");
            DisconnectTcp();
        }
    }

    private void DisconnectTcp()
    {
        if (receiveCancel != null)
        {
            receiveCancel.Cancel();
            receiveCancel.Dispose();
            receiveCancel = null;
        }

        if (stream != null)
        {
            stream.Close();
            stream = null;
        }

        if (client != null)
        {
            client.Close();
            client = null;
        }

        SetConnected(false);
    }

    private void SetConnected(bool connected)
    {
        if (InvokeRequired)
        {
            BeginInvoke((Action)(() => SetConnected(connected)));
            return;
        }

        connectButton.Enabled = !connected;
        disconnectButton.Enabled = connected;
        sendButton.Enabled = connected;
        statusLabel.Text = connected ? "Connected" : "Disconnected";
    }

    private void AppendLog(string text)
    {
        if (InvokeRequired)
        {
            BeginInvoke((Action)(() => AppendLog(text)));
            return;
        }

        logBox.AppendText(text);
        logBox.SelectionStart = logBox.TextLength;
        logBox.ScrollToCaret();
    }
}

public static class ArexTcpDebugLauncher
{
    [STAThread]
    public static void Main()
    {
        Application.EnableVisualStyles();
        Application.SetCompatibleTextRenderingDefault(false);
        Application.Run(new ArexTcpDebugForm());
    }
}
'@

Add-Type -TypeDefinition $source -ReferencedAssemblies System.Windows.Forms,System.Drawing,System.dll

if ($env:AREX_TCP_DEBUG_COMPILE_ONLY -ne "1") {
    [ArexTcpDebugLauncher]::Main()
}
