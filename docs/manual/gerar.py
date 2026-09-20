# =============================================================================
# Gera o manual.html a partir do modelo.html + das capturas em img/
# =============================================================================
# O manual mostra o sistema como ele e, entao as imagens saem do programa de
# verdade. Quando uma tela mudar, refaca as duas etapas:
#
#   1) capturar as telas (do diretorio raiz do projeto):
#        set QT_QPA_PLATFORM=offscreen
#        set QT_QUICK_BACKEND=software
#        build\mingw\capturar_telas.exe docs\manual\img
#
#   2) montar o manual:
#        python docs\manual\gerar.py
#
# O manual.html resultante e AUTOSSUFICIENTE: as imagens vao embutidas, entao
# ele abre em qualquer navegador, sem internet e sem a pasta img/ do lado.
# =============================================================================

import base64
import io
import os
import re
import sys

AQUI = os.path.dirname(os.path.abspath(__file__))
MODELO = os.path.join(AQUI, "modelo.html")
IMAGENS = os.path.join(AQUI, "img")
SAIDA = os.path.join(AQUI, "manual.html")

MARCADOR = r"__IMG:([a-z0-9\-]+)__"


def main():
    with io.open(MODELO, encoding="utf-8") as f:
        modelo = f.read()

    usadas = set(re.findall(MARCADOR, modelo))
    disponiveis = {n[:-4] for n in os.listdir(IMAGENS) if n.endswith(".png")}

    faltando = sorted(usadas - disponiveis)
    if faltando:
        # Falhar alto: um manual com imagem quebrada e pior do que nenhum.
        sys.exit("Imagens citadas no modelo e inexistentes em img/: %s" % faltando)

    sobrando = sorted(disponiveis - usadas)
    if sobrando:
        print("Aviso: capturadas mas nao usadas no manual: %s" % sobrando)

    def embutir(m):
        caminho = os.path.join(IMAGENS, m.group(1) + ".png")
        with open(caminho, "rb") as img:
            dados = img.read()
        if dados[:8] != b"\x89PNG\r\n\x1a\n":
            sys.exit("%s nao e um PNG" % caminho)
        return "data:image/png;base64," + base64.b64encode(dados).decode("ascii")

    saida = re.sub(MARCADOR, embutir, modelo)

    with io.open(SAIDA, "w", encoding="utf-8") as f:
        f.write(saida)

    tamanho = len(saida.encode("utf-8")) / 1048576.0
    print("manual.html gerado: %d imagens, %.2f MB" % (len(usadas), tamanho))
    # 16 MB e o teto de uma pagina publicada como Artifact.
    if tamanho > 15:
        print("ATENCAO: perto do limite de 16 MB de uma pagina publicada.")


if __name__ == "__main__":
    main()
