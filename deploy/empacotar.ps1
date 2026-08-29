# =============================================================================
# empacotar.ps1 — gera o pacote instalável do Empório dos Amigos
# =============================================================================
# Produz uma pasta AUTOSSUFICIENTE (com o Qt junto) e um .zip, para copiar no
# PC da loja e rodar SEM instalar nada — nem o Qt.
#
# Uso (no PowerShell, a partir da raiz do projeto):
#     .\deploy\empacotar.ps1
#
# Parâmetros opcionais:
#     -Qt      caminho do Qt        (padrão: C:\Qt\6.8.3\mingw_64)
#     -MinGW   caminho do compilador(padrão: C:\Qt\Tools\mingw1310_64\bin)
#     -Saida   pasta de saída       (padrão: deploy\pacote)
# =============================================================================
param(
    [string]$Qt     = "C:\Qt\6.8.3\mingw_64",
    [string]$MinGW  = "C:\Qt\Tools\mingw1310_64\bin",
    [string]$CMake  = "C:\Qt\Tools\CMake_64\bin\cmake.exe",
    [string]$Saida  = ""
)

# O PowerShell 5.1 trata QUALQUER stderr de programa externo como erro — e o
# windeployqt escreve avisos inofensivos ali. Por isso conferimos o codigo de
# saida na mao, em vez de parar no primeiro ruido.
$ErrorActionPreference = "Continue"
function Exec($desc, $bloco) {
    & $bloco 2>&1 | Out-String | Write-Verbose
    if ($LASTEXITCODE -ne 0) { throw "$desc falhou (codigo $LASTEXITCODE)" }
}
$raiz = Split-Path -Parent $PSScriptRoot
if ([string]::IsNullOrEmpty($Saida)) { $Saida = Join-Path $raiz "deploy\pacote" }
$build = Join-Path $raiz "build\release"
$app   = "Emporio dos Amigos"

Write-Host "== Empacotando o $app ==" -ForegroundColor Cyan

# Pré-requisitos: sem eles o resto falha de formas confusas.
foreach ($p in @($Qt, $MinGW, $CMake)) {
    if (-not (Test-Path $p)) { throw "Nao encontrei: $p  (ajuste os parametros do script)" }
}
$env:PATH = "$Qt\bin;$MinGW;$env:PATH"

# --- 1. Compila em Release (sem os testes) --------------------------------
Write-Host "`n[1/4] Compilando em Release..." -ForegroundColor Yellow
$qtCMake = $Qt -replace '\\', '/'
Exec "Configuracao do CMake" {
    & $CMake -S $raiz -B $build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="$qtCMake"
}
Exec "Compilacao" { & $CMake --build $build }
$exe = Join-Path $build "distribuidora.exe"
if (-not (Test-Path $exe)) { throw "A compilacao nao gerou o distribuidora.exe" }

# --- 2. Monta a pasta do pacote -------------------------------------------
Write-Host "[2/4] Montando a pasta do pacote..." -ForegroundColor Yellow
if (Test-Path $Saida) { Remove-Item -Recurse -Force $Saida }
New-Item -ItemType Directory -Force -Path $Saida | Out-Null
Copy-Item $exe (Join-Path $Saida "$app.exe")

# --- 3. Traz o Qt junto (DLLs, plugins e QML) -----------------------------
# --qmldir faz o windeployqt descobrir os módulos QML realmente usados.
Write-Host "[3/4] Copiando o Qt (windeployqt)..." -ForegroundColor Yellow
$alvo = Join-Path $Saida "$app.exe"
$qmlDir = Join-Path $raiz "qml"
Exec "windeployqt" {
    & "$Qt\bin\windeployqt.exe" --release --qmldir $qmlDir `
        --no-translations --no-system-d3d-compiler --no-opengl-sw $alvo
}

# Runtime do MinGW (o windeployqt nem sempre traz).
foreach ($dll in @("libgcc_s_seh-1.dll", "libstdc++-6.dll", "libwinpthread-1.dll")) {
    $o = Join-Path $MinGW $dll
    if (Test-Path $o) { Copy-Item $o $Saida -Force }
}

# Instruções para quem vai instalar na loja.
@"
EMPORIO DOS AMIGOS - Sistema de Gestao
=======================================

COMO INSTALAR NO COMPUTADOR DA LOJA

1. Copie esta pasta inteira para o computador, por exemplo em:
       C:\Emporio dos Amigos

2. Abra a pasta e clique com o botao direito em "$app.exe"
   -> Enviar para -> Area de trabalho (criar atalho)

3. Pronto. Use o atalho da area de trabalho para abrir o sistema.

Nao precisa instalar mais nada: tudo o que o sistema usa ja esta nesta pasta.

PRIMEIRO ACESSO
   O sistema pede para criar o usuario administrador (nome, login e senha).
   Guarde essa senha: e ela que libera relatorios, backup e cancelamento.

ONDE FICAM OS DADOS
   Os dados NAO ficam nesta pasta. Ficam em:
       %APPDATA%\Distribuidora\Distribuidora\distribuidora.db
   Assim voce pode atualizar o sistema (trocar esta pasta) sem perder nada.

BACKUP
   Ao fechar o caixa o sistema faz backup sozinho e guarda as 5 copias mais
   recentes em Documentos\Emporio dos Amigos\Backups.
   Na tela "Backup" da para fazer copia na hora e restaurar.

AVISO NO CELULAR
   Na tela "Backup" configure o Telegram para receber o resumo do dia no
   celular ao fechar o caixa.

SE ALGO DER ERRADO
   O sistema grava tudo em:
       %APPDATA%\Distribuidora\Distribuidora\logs\sistema.log
   Na tela "Backup" ha um botao "Abrir pasta" para pegar esse arquivo e
   enviar a quem da suporte.

GUIA COMPLETO
   Passo a passo detalhado (configuracao inicial, Telegram, backup,
   atualizacao e problemas comuns) em docs/instalacao.md no repositorio.
"@ | Out-File -FilePath (Join-Path $Saida "LEIA-ME.txt") -Encoding utf8

# --- 3b. Carimbo de versao -------------------------------------------------
# A pasta vai versionada no Git. Sem este arquivo, ninguem consegue saber se o
# executavel ali dentro e o do codigo atual ou de tres semanas atras.
$commit = (& git -C $raiz rev-parse --short HEAD 2>$null)
if (-not $commit) { $commit = "(fora de um repositorio git)" }
$sujo = (& git -C $raiz status --porcelain 2>$null | Where-Object { $_ -notmatch 'deploy/pacote' })
$estado = if ($sujo) { "COM ALTERACOES NAO COMMITADAS" } else { "limpo" }
@"
$app - pacote pronto para rodar
=======================================

Gerado em : $(Get-Date -Format 'dd/MM/yyyy HH:mm')
Commit    : $commit
Arvore    : $estado

Este executavel foi compilado a partir do commit acima. Se voce alterou o
codigo depois disso, rode deploy\empacotar.ps1 de novo antes de levar para a
loja - senao estara instalando uma versao velha.
"@ | Out-File -FilePath (Join-Path $Saida "VERSAO.txt") -Encoding utf8

# --- 4. Compacta ----------------------------------------------------------
Write-Host "[4/4] Compactando..." -ForegroundColor Yellow
$zip = Join-Path $raiz ("deploy\{0}.zip" -f ($app -replace ' ','-'))
if (Test-Path $zip) { Remove-Item -Force $zip }
Compress-Archive -Path (Join-Path $Saida "*") -DestinationPath $zip

$mb = [math]::Round((Get-Item $zip).Length / 1MB, 1)
$n  = (Get-ChildItem $Saida -Recurse -File).Count
Write-Host "`nPacote pronto!" -ForegroundColor Green
Write-Host "   Pasta : $Saida  ($n arquivos)"
Write-Host "   Zip   : $zip  ($mb MB)"
Write-Host "`nCopie a pasta (ou o zip) para o PC da loja e rode o `"$app.exe`"." -ForegroundColor Cyan
