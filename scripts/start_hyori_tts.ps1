param(
    [string]$GptSovitsRoot = 'D:\Program\project\GPT-SoVITS-v2pro-20250604',
    [int]$Port = 9880
)

$ErrorActionPreference = 'Stop'
$resolvedRoot = (Resolve-Path -LiteralPath $GptSovitsRoot).Path
$python = Join-Path $resolvedRoot 'runtime\python.exe'
$api = Join-Path $resolvedRoot 'api_v2.py'
$gptWeight = Join-Path $resolvedRoot 'GPT_weights_v2Pro\hyori_v2pro-e10.ckpt'
$sovitsWeight = Join-Path $resolvedRoot 'SoVITS_weights_v2Pro\hyori_v2pro_e8_s1008.pth'
$configPath = Join-Path $resolvedRoot 'TEMP\hyori_tts_infer.yaml'

foreach ($requiredPath in @($python, $api, $gptWeight, $sovitsWeight)) {
    if (-not (Test-Path -LiteralPath $requiredPath)) {
        throw "Required file not found: $requiredPath"
    }
}

$gptWeightYaml = $gptWeight.Replace('\', '/')
$sovitsWeightYaml = $sovitsWeight.Replace('\', '/')
$yaml = @"
custom:
  bert_base_path: GPT_SoVITS/pretrained_models/chinese-roberta-wwm-ext-large
  cnhuhbert_base_path: GPT_SoVITS/pretrained_models/chinese-hubert-base
  device: cuda
  is_half: true
  t2s_weights_path: "$gptWeightYaml"
  version: v2Pro
  vits_weights_path: "$sovitsWeightYaml"
"@

Set-Content -LiteralPath $configPath -Value $yaml -Encoding UTF8
$env:Path = "$(Join-Path $resolvedRoot 'runtime');$env:Path"

Push-Location $resolvedRoot
try {
    Write-Host "Starting Hyori GPT-SoVITS API at http://127.0.0.1:$Port"
    & $python $api -a 127.0.0.1 -p $Port -c $configPath
}
finally {
    Pop-Location
}
