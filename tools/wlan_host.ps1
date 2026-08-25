# CordOS host Wi-Fi radio (paravirtual).
# Speaks a line protocol on a named pipe so the guest WLAN driver can
# scan the SSIDs the Windows host actually sees. Does NOT switch the
# host's association (that would drop this session).
#
# Protocol (COM2 / \\.\pipe\cordos-wlan):
#   guest: S
#   host:  W1
#          <ssid>\t<quality 0-100>\t<sec 0=open 1=wep 2=wpa>
#          .
#   guest: C\t<ssid>\t<password>
#   host:  K | F

$ErrorActionPreference = "Continue"
$PipeName = "cordos-wlan"
$Root = Split-Path -Parent $PSScriptRoot
$LogDir = Join-Path $Root "out"
if (-not (Test-Path -LiteralPath $LogDir)) {
    New-Item -ItemType Directory -Path $LogDir | Out-Null
}
$LogPath = Join-Path $LogDir "wlan_host.log"

function Write-Log([string]$msg) {
    $line = "[{0}] {1}" -f (Get-Date -Format "HH:mm:ss"), $msg
    Add-Content -LiteralPath $LogPath -Value $line -Encoding ASCII
}

function Get-HostWifi {
    $list = New-Object System.Collections.Generic.List[object]
    $raw = $null
    try {
        $raw = & netsh wlan show networks mode=bssid 2>$null
    } catch {
        Write-Log "netsh failed: $_"
        return $list
    }
    if ($null -eq $raw) {
        return $list
    }

    $ssid = $null
    $auth = "OPEN"
    $signal = 0
    foreach ($line in $raw) {
        $s = [string]$line
        if ($s -match '^\s*SSID\s+\d+\s*:\s*(.*)\s*$') {
            if ($ssid) {
                $list.Add([pscustomobject]@{ Ssid = $ssid; Quality = $signal; Auth = $auth })
            }
            $ssid = $Matches[1].Trim()
            if ([string]::IsNullOrWhiteSpace($ssid)) {
                $ssid = $null
            }
            $auth = "OPEN"
            $signal = 0
        } elseif ($s -match '(?i)(Authentication|Autenticaci[oó]n)\s*:\s*(.+)\s*$') {
            $auth = $Matches[2].Trim()
        } elseif ($s -match '(?i)(Signal|Se[nñ]al)\s*:\s*(\d+)\s*%') {
            $n = [int]$Matches[2]
            if ($n -gt $signal) { $signal = $n }
        }
    }
    if ($ssid) {
        $list.Add([pscustomobject]@{ Ssid = $ssid; Quality = $signal; Auth = $auth })
    }
    return $list
}

function Sec-Code([string]$auth) {
    $a = $auth.ToUpperInvariant()
    if ($a -match 'OPEN' -or $a -match 'ABIERT') { return 0 }
    if ($a -match 'WEP') { return 1 }
    return 2
}

function Sanitize([string]$t) {
    if ($null -eq $t) { return "" }
    return (($t -replace '[\t\r\n]', ' ').Trim())
}

function Read-PipeLine($stream) {
    $sb = New-Object System.Text.StringBuilder
    while ($true) {
        $b = $stream.ReadByte()
        if ($b -lt 0) { return $null }
        if ($b -eq 10) { break }
        if ($b -eq 13) { continue }
        [void]$sb.Append([char]$b)
        if ($sb.Length -gt 400) { break }
    }
    return $sb.ToString()
}

function Write-PipeLine($stream, [string]$text) {
    $bytes = [Text.Encoding]::ASCII.GetBytes($text + "`n")
    $stream.Write($bytes, 0, $bytes.Length)
    $stream.Flush()
}

Write-Log "start client pipe=$PipeName"

$last = @()
for ($round = 0; $round -lt 256; $round++) {
    $client = $null
    try {
        $client = New-Object System.IO.Pipes.NamedPipeClientStream(
            ".",
            $PipeName,
            [System.IO.Pipes.PipeDirection]::InOut,
            [System.IO.Pipes.PipeOptions]::None)
        $client.Connect(2000)
    } catch {
        if ($client) { try { $client.Dispose() } catch {} }
        Start-Sleep -Milliseconds 400
        continue
    }
    if (-not $client.IsConnected) {
        try { $client.Dispose() } catch {}
        Start-Sleep -Milliseconds 400
        continue
    }

    Write-Log "connected to vbox"
    try {
        while ($client.IsConnected) {
            $cmd = Read-PipeLine $client
            if ($null -eq $cmd) { break }
            $cmd = $cmd.Trim()
            if ($cmd.Length -eq 0) { continue }
            $head = $cmd.Substring(0, 1).ToUpperInvariant()
            if ($head -eq 'S') {
                $last = @(Get-HostWifi)
                Write-Log ("scan {0} nets" -f $last.Count)
                Write-PipeLine $client "W1"
                $seen = @{}
                foreach ($n in $last) {
                    $id = Sanitize $n.Ssid
                    if ($id.Length -eq 0) { continue }
                    if ($seen.ContainsKey($id)) { continue }
                    $seen[$id] = $true
                    $q = [int]$n.Quality
                    if ($q -lt 0) { $q = 0 }
                    if ($q -gt 100) { $q = 100 }
                    $sec = Sec-Code $n.Auth
                    Write-PipeLine $client ("{0}`t{1}`t{2}" -f $id, $q, $sec)
                }
                Write-PipeLine $client "."
            } elseif ($head -eq 'C') {
                $parts = $cmd.Split("`t")
                $want = ""
                if ($parts.Count -ge 2) { $want = Sanitize $parts[1] }
                $ok = $false
                foreach ($n in $last) {
                    if ((Sanitize $n.Ssid) -eq $want) { $ok = $true; break }
                }
                if (-not $ok -and $want.Length -gt 0 -and $last.Count -eq 0) {
                    $last = @(Get-HostWifi)
                    foreach ($n in $last) {
                        if ((Sanitize $n.Ssid) -eq $want) { $ok = $true; break }
                    }
                }
                Write-Log ("connect '{0}' -> {1}" -f $want, $ok)
                if ($ok) { Write-PipeLine $client "K" } else { Write-PipeLine $client "F" }
            }
        }
    } catch {
        Write-Log "session: $_"
    } finally {
        try { $client.Dispose() } catch {}
        Write-Log "disconnected"
    }
}
Write-Log "exit"
