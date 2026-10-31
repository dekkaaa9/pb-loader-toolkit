# ============================================================
# PRE-ENCRYPT CRACKED DLL
# The loader XOR-decrypts the downloaded payload with key:
#   "325768235246856876125"
# So we pre-encrypt the cracked DLL with the same key.
# When the loader decrypts our file, it produces the cracked DLL.
# ============================================================

# ------------------------------------------------------------
# Cracked by dekkaaa9  |  https://github.com/dekkaaa9
# Tools: REA, Ghidra, PowerShell, llvm-mingw, Win32 API, DirectX 9
# ------------------------------------------------------------
$crackedDll = [System.IO.File]::ReadAllBytes('D:\pbct\decrypted_payload_cracked.dll')
$key = [System.Text.Encoding]::ASCII.GetBytes('325768235246856876125')

$preEncrypted = New-Object byte[] $crackedDll.Length
for ($i = 0; $i -lt $crackedDll.Length; $i++) {
    $preEncrypted[$i] = $crackedDll[$i] -bxor $key[$i % $key.Length]
}

[System.IO.File]::WriteAllBytes('D:\PB_Sumatra\payload_preencrypted.bin', $preEncrypted)

Write-Output "Pre-encrypted cracked DLL saved to: D:\PB_Sumatra\payload_preencrypted.bin"
Write-Output "Size: $($preEncrypted.Length) bytes"
Write-Output "SHA256: $((Get-FileHash 'D:\PB_Sumatra\payload_preencrypted.bin' -Algorithm SHA256).Hash)"
Write-Output ""
Write-Output "This file, when XOR-decrypted by the loader with key '325768235246856876125',"
Write-Output "produces the CRACKED DLL (key validation bypassed)."