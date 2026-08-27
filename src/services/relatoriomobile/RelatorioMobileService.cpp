#include "services/relatoriomobile/RelatorioMobileService.h"

#include <algorithm>
#include <utility>

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <QSqlQuery>

#include "domain/caixa/CaixaRepository.h"
#include "domain/clientes/ClienteRepository.h"
#include "domain/compras/CompraRepository.h"
#include "domain/estoque/EstoqueRepository.h"
#include "domain/financeiro/FinanceiroRepository.h"
#include "domain/relatorios/RelatorioRepository.h"
#include "utils/Money.h"

RelatorioMobileService::RelatorioMobileService(QSqlDatabase db, const QString &destinoDir)
    : m_db(std::move(db))
    , m_dir(destinoDir.isEmpty() ? pastaPadrao() : destinoDir)
{
}

QString RelatorioMobileService::pastaPadrao()
{
    // A variável de ambiente OneDrive aponta para a pasta local sincronizada.
    QString base = qEnvironmentVariable("OneDrive");
    if (base.isEmpty())
        base = qEnvironmentVariable("OneDriveConsumer");
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/Documentos");
    return QDir::fromNativeSeparators(base) + QStringLiteral("/Empório dos Amigos/Relatório");
}

QString RelatorioMobileService::caminhoArquivo() const
{
    return m_dir + QStringLiteral("/relatorio.html");
}

bool RelatorioMobileService::gerar(QString *out)
{
    if (!QDir().mkpath(m_dir)) {
        m_erro = QStringLiteral("Não foi possível criar a pasta do relatório: %1").arg(m_dir);
        return false;
    }
    const QString html = montarHtml();
    QFile f(caminhoArquivo());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_erro = QStringLiteral("Não foi possível gravar o relatório: %1").arg(f.errorString());
        return false;
    }
    f.write(html.toUtf8());
    f.close();
    if (out)
        *out = caminhoArquivo();
    m_erro.clear();
    return true;
}

QString RelatorioMobileService::montarHtml() const
{
    // ---- Coleta de dados (reaproveita os repositórios) ----
    QJsonObject dados;
    dados[QStringLiteral("loja")] = QStringLiteral("Empório dos Amigos");
    dados[QStringLiteral("atualizadoEm")] = QDateTime::currentDateTime().toString(Qt::ISODate);

    RelatorioRepository rel(m_db);
    QJsonObject periodos;
    for (int dias : {0, 7, 30}) {
        const FaturamentoResumo f = rel.faturamento(dias);
        QJsonObject p;
        p[QStringLiteral("faturamento")] = Money::format(f.total);
        p[QStringLiteral("lucro")] = Money::format(f.lucro);
        p[QStringLiteral("numVendas")] = f.numVendas;
        p[QStringLiteral("ticket")] = Money::format(f.ticket);

        QJsonArray formas;
        for (const FormaTotal &ft : rel.vendasPorForma(dias)) {
            QJsonObject o;
            o[QStringLiteral("forma")] = ft.forma;
            o[QStringLiteral("valor")] = Money::format(ft.total);
            formas.append(o);
        }
        p[QStringLiteral("formas")] = formas;

        QJsonArray mv;
        for (const ProdutoVendido &pv : rel.maisVendidos(dias, 8)) {
            QJsonObject o;
            o[QStringLiteral("nome")] = pv.nome;
            o[QStringLiteral("qtd")] = static_cast<double>(pv.qtd);
            mv.append(o);
        }
        p[QStringLiteral("maisVendidos")] = mv;

        periodos[QString::number(dias)] = p;
    }
    dados[QStringLiteral("periodos")] = periodos;

    // Produtos parados (30 dias) — o que não gira.
    QJsonArray paradosArr;
    for (const ProdutoParado &pp : rel.produtosParados(30)) {
        QJsonObject o;
        o[QStringLiteral("nome")] = pp.nome;
        o[QStringLiteral("estoque")] = static_cast<double>(pp.estoque);
        paradosArr.append(o);
    }
    dados[QStringLiteral("parados")] = paradosArr;

    // Estoque (baixos primeiro) + valor imobilizado.
    EstoqueRepository est(m_db);
    QVector<ItemEstoque> itens = est.listar();
    std::sort(itens.begin(), itens.end(), [](const ItemEstoque &a, const ItemEstoque &b) {
        const bool ba = a.quantidade <= a.minimo;
        const bool bb = b.quantidade <= b.minimo;
        if (ba != bb) return ba;                    // baixos primeiro
        return a.nome.localeAwareCompare(b.nome) < 0;
    });
    QJsonArray estArr;
    qint64 valorEstoque = 0;
    int emFalta = 0;
    for (const ItemEstoque &it : itens) {
        QJsonObject o;
        o[QStringLiteral("nome")] = it.nome;
        o[QStringLiteral("quantidade")] = static_cast<double>(it.quantidade);
        o[QStringLiteral("unidade")] = it.unidadeBase;
        o[QStringLiteral("custoMedio")] = Money::format(it.custoMedio);
        o[QStringLiteral("baixo")] = (it.quantidade <= it.minimo);
        estArr.append(o);
        valorEstoque += it.quantidade * it.custoMedio;
        if (it.quantidade <= it.minimo)
            ++emFalta;
    }
    dados[QStringLiteral("estoque")] = estArr;
    dados[QStringLiteral("estoqueValor")] = Money::format(valorEstoque);
    dados[QStringLiteral("estoqueEmFalta")] = emFalta;
    dados[QStringLiteral("estoqueItens")] = itens.size();

    // Último fechamento de caixa (este relatório nasce justamente no fechamento).
    {
        QJsonObject cx;
        int sid = 0;
        QSqlQuery q(m_db);
        if (q.exec(QStringLiteral(
                "SELECT id, valor_esperado, valor_informado, diferenca, fechada_em "
                "FROM sessoes_caixa WHERE status='fechada' ORDER BY id DESC LIMIT 1"))
            && q.next()) {
            sid = q.value(0).toInt();
            cx[QStringLiteral("esperado")] = Money::format(q.value(1).toLongLong());
            cx[QStringLiteral("contado")] = Money::format(q.value(2).toLongLong());
            const qint64 dif = q.value(3).toLongLong();
            cx[QStringLiteral("diferenca")] = Money::format(dif);
            cx[QStringLiteral("difValor")] = static_cast<double>(dif);
            cx[QStringLiteral("fechadaEm")] = q.value(4).toString();
        }
        if (sid > 0) {
            CaixaRepository caixa(m_db);
            const ResumoCaixa rc = caixa.resumo(sid);
            cx[QStringLiteral("vendido")] = Money::format(rc.totalVendas());
            cx[QStringLiteral("numVendas")] = rc.numVendas;
            cx[QStringLiteral("abertura")] = Money::format(rc.abertura);
            cx[QStringLiteral("dinheiro")] = Money::format(rc.vendasDinheiro);
            cx[QStringLiteral("sangrias")] = Money::format(rc.sangrias);
            cx[QStringLiteral("suprimentos")] = Money::format(rc.suprimentos);
            cx[QStringLiteral("recebimentos")] = Money::format(rc.recebimentos);
            cx[QStringLiteral("temDados")] = true;
        } else {
            cx[QStringLiteral("temDados")] = false;
        }
        dados[QStringLiteral("caixa")] = cx;
    }

    // Contas a pagar em aberto.
    FinanceiroRepository fin(m_db);
    {
        QJsonArray arr;
        qint64 total = 0;
        for (const ContaPagar &cp : fin.contasPagar(/*apenasAbertas=*/true)) {
            QJsonObject o;
            o[QStringLiteral("descricao")] = cp.descricao;
            o[QStringLiteral("fornecedor")] = cp.fornecedorNome;
            o[QStringLiteral("valor")] = Money::format(cp.valor);
            o[QStringLiteral("vencimento")] = cp.vencimento;
            o[QStringLiteral("vencida")] = cp.vencida;
            arr.append(o);
            total += cp.valor;
        }
        QJsonObject ap;
        ap[QStringLiteral("total")] = Money::format(total);
        ap[QStringLiteral("contas")] = arr;
        dados[QStringLiteral("aPagar")] = ap;
    }

    // Últimas compras registradas.
    {
        QJsonArray arr;
        int n = 0;
        for (const CompraResumo &cr : CompraRepository(m_db).listar()) {
            if (n++ >= 10)
                break;
            QJsonObject o;
            o[QStringLiteral("fornecedor")] = cr.fornecedorNome;
            o[QStringLiteral("data")] = cr.data;
            o[QStringLiteral("total")] = Money::format(cr.total);
            o[QStringLiteral("itens")] = cr.numItens;
            o[QStringLiteral("nota")] = cr.numeroNota;
            arr.append(o);
        }
        dados[QStringLiteral("compras")] = arr;
    }

    // Fiado a receber (por cliente com saldo).
    ClienteRepository cli(m_db);
    QJsonArray fiadoArr;
    qint64 fiadoTotal = 0;
    for (const Cliente &c : cli.listar()) {
        if (c.saldoDevedor > 0) {
            QJsonObject o;
            o[QStringLiteral("nome")] = c.nome;
            o[QStringLiteral("saldo")] = Money::format(c.saldoDevedor);
            fiadoArr.append(o);
            fiadoTotal += c.saldoDevedor;
        }
    }
    QJsonObject fiado;
    fiado[QStringLiteral("total")] = Money::format(fiadoTotal);
    fiado[QStringLiteral("clientes")] = fiadoArr;
    dados[QStringLiteral("fiado")] = fiado;

    QString json = QString::fromUtf8(
        QJsonDocument(dados).toJson(QJsonDocument::Compact));
    // Escapa '<' (só aparece dentro de strings do JSON) para impedir qualquer
    // fechamento acidental de </script> vindo de um nome de produto/cliente.
    json.replace(QLatin1Char('<'), QStringLiteral("\\u003c"));

    // ---- Template HTML (autossuficiente: CSS + JS embutidos) ----
    static const QString kTemplate = QStringLiteral(R"HTMLDOC(<!doctype html>
<html lang="pt-BR">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1, viewport-fit=cover">
<title>Empório dos Amigos — Relatório</title>
<style>
  :root{ --bg:#000; --card:#121019; --card2:#0d0b12; --line:#2a2620; --txt:#F2EEE7; --muted:#9A9285; --orange:#F0742E; --green:#63C48C; --red:#E8877A; }
  *{ box-sizing:border-box; -webkit-tap-highlight-color:transparent; }
  body{ margin:0; background:#000; color:var(--txt); font-family:-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,Arial,sans-serif; padding:0 0 40px; }
  header{ padding:20px 16px 12px; }
  header h1{ margin:0; font-size:22px; letter-spacing:.2px; }
  header .att{ color:var(--muted); font-size:12px; margin-top:2px; }
  .tabs{ display:flex; gap:8px; padding:8px 16px 4px; overflow:auto; }
  .tab{ flex:0 0 auto; padding:8px 14px; border-radius:999px; background:#161018; color:var(--muted); font-weight:600; font-size:13px; border:1px solid var(--line); }
  .tab.on{ background:var(--orange); color:#150f08; border-color:var(--orange); }
  .wrap{ padding:8px 16px; }
  .kpis{ display:grid; grid-template-columns:1fr 1fr; gap:10px; }
  .kpi{ background:var(--card); border:1px solid var(--line); border-radius:14px; padding:12px 14px; }
  .kpi .l{ color:var(--muted); font-size:11px; text-transform:uppercase; letter-spacing:.5px; font-weight:700; }
  .kpi .v{ font-size:21px; font-weight:800; margin-top:4px; }
  .kpi.big .v{ color:var(--orange); }
  h2{ font-size:13px; text-transform:uppercase; letter-spacing:.6px; color:var(--muted); margin:22px 4px 8px; }
  .card{ background:var(--card); border:1px solid var(--line); border-radius:14px; overflow:hidden; }
  .row{ display:flex; align-items:center; justify-content:space-between; padding:11px 14px; border-bottom:1px solid var(--line); gap:10px; }
  .row:last-child{ border-bottom:0; }
  .row .n{ font-weight:600; min-width:0; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
  .row .s{ color:var(--muted); font-size:12px; }
  .row .val{ font-weight:700; white-space:nowrap; }
  .pill{ font-size:11px; font-weight:800; color:var(--red); border:1px solid var(--red); border-radius:999px; padding:1px 8px; }
  .empty{ color:var(--muted); padding:16px; text-align:center; font-size:13px; }
  .foot{ color:var(--muted); font-size:11px; text-align:center; margin-top:24px; padding:0 16px; }
</style>
</head>
<body>
  <header>
    <h1 id="loja">Empório dos Amigos</h1>
    <div class="att" id="att"></div>
  </header>
  <div class="tabs" id="tabs">
    <div class="tab on" data-p="0">Hoje</div>
    <div class="tab" data-p="7">7 dias</div>
    <div class="tab" data-p="30">30 dias</div>
  </div>
  <div class="wrap">
    <div class="kpis">
      <div class="kpi big"><div class="l">Faturamento</div><div class="v" id="kFat">—</div></div>
      <div class="kpi big"><div class="l">Lucro</div><div class="v" id="kLucro">—</div></div>
      <div class="kpi"><div class="l">Vendas</div><div class="v" id="kNum">—</div></div>
      <div class="kpi"><div class="l">Ticket médio</div><div class="v" id="kTicket">—</div></div>
    </div>

    <h2>Formas de pagamento</h2>
    <div class="card" id="formas"></div>

    <h2>Mais vendidos</h2>
    <div class="card" id="maisVendidos"></div>

    <h2 id="hCaixa">Último fechamento de caixa</h2>
    <div class="card" id="caixa"></div>

    <h2 id="hEstoque">Estoque</h2>
    <div class="card" id="estoque"></div>

    <h2 id="hFiado">A receber (fiado)</h2>
    <div class="card" id="fiado"></div>

    <h2 id="hPagar">A pagar</h2>
    <div class="card" id="pagar"></div>

    <h2>Últimas compras</h2>
    <div class="card" id="compras"></div>

    <h2>Parados (30 dias sem vender)</h2>
    <div class="card" id="parados"></div>

    <div class="foot">Empório dos Amigos · relatório somente leitura</div>
  </div>

  <script id="dados" type="application/json">%DADOS%</script>
  <script>
    const D = JSON.parse(document.getElementById('dados').textContent);
    let periodo = '0';
    const $ = (id) => document.getElementById(id);
    const cap = (s) => s ? s.charAt(0).toUpperCase()+s.slice(1) : s;
    function fmtData(iso){ try{ const d=new Date(iso); return d.toLocaleDateString('pt-BR')+' '+d.toLocaleTimeString('pt-BR',{hour:'2-digit',minute:'2-digit'}); }catch(e){ return iso; } }
    function rows(el, arr, mk){ if(!arr||!arr.length){ el.innerHTML='<div class="empty">Nada por aqui.</div>'; return; } el.innerHTML = arr.map(mk).join(''); }

    $('loja').textContent = D.loja;
    $('att').textContent = 'Atualizado em ' + fmtData(D.atualizadoEm);

    function renderPeriodo(){
      const p = D.periodos[periodo] || {};
      $('kFat').textContent = p.faturamento||'—';
      $('kLucro').textContent = p.lucro||'—';
      $('kNum').textContent = (p.numVendas!=null? p.numVendas : '—');
      $('kTicket').textContent = p.ticket||'—';
      rows($('formas'), p.formas, f => `<div class="row"><div class="n">${cap(f.forma)}</div><div class="val">${f.valor}</div></div>`);
      rows($('maisVendidos'), p.maisVendidos, m => `<div class="row"><div class="n">${m.nome}</div><div class="val">${m.qtd}</div></div>`);
    }

    // --- Seções que não dependem do período ---

    // Último fechamento de caixa.
    (function(){
      const cx = D.caixa||{};
      if (!cx.temDados) { $('caixa').innerHTML = '<div class="empty">Nenhum caixa fechado ainda.</div>'; return; }
      $('hCaixa').textContent = 'Último fechamento de caixa · ' + fmtData(cx.fechadaEm);
      const dif = cx.difValor||0;
      const corDif = dif === 0 ? 'var(--green)' : (dif < 0 ? 'var(--red)' : 'var(--orange)');
      const rotuloDif = dif === 0 ? 'confere' : (dif < 0 ? 'falta' : 'sobra');
      $('caixa').innerHTML =
        `<div class="row"><div class="n">Vendido no turno (${cx.numVendas||0})</div><div class="val">${cx.vendido||'—'}</div></div>`
      + `<div class="row"><div class="n">Abertura</div><div class="val">${cx.abertura||'—'}</div></div>`
      + `<div class="row"><div class="n">Vendas em dinheiro</div><div class="val">${cx.dinheiro||'—'}</div></div>`
      + `<div class="row"><div class="n">Suprimentos</div><div class="val">${cx.suprimentos||'—'}</div></div>`
      + `<div class="row"><div class="n">Recebimentos de fiado</div><div class="val">${cx.recebimentos||'—'}</div></div>`
      + `<div class="row"><div class="n">Sangrias</div><div class="val">${cx.sangrias||'—'}</div></div>`
      + `<div class="row"><div class="n">Esperado na gaveta</div><div class="val">${cx.esperado||'—'}</div></div>`
      + `<div class="row"><div class="n">Contado</div><div class="val">${cx.contado||'—'}</div></div>`
      + `<div class="row"><div class="n" style="color:${corDif}">Diferença (${rotuloDif})</div><div class="val" style="color:${corDif}">${cx.diferenca||'—'}</div></div>`;
    })();

    // Estoque (com valor imobilizado e quantos estão baixos).
    $('hEstoque').textContent = 'Estoque · ' + (D.estoqueValor||'R$ 0,00')
      + (D.estoqueEmFalta ? ('  ·  ' + D.estoqueEmFalta + ' em falta') : '');
    rows($('estoque'), D.estoque, e => `<div class="row"><div><div class="n">${e.nome}</div><div class="s">custo ${e.custoMedio}</div></div><div style="display:flex;align-items:center;gap:8px">${e.baixo?'<span class="pill">baixo</span>':''}<span class="val">${e.quantidade} ${e.unidade||''}</span></div></div>`);

    // Fiado a receber.
    (function(){
      const fi = D.fiado||{clientes:[]};
      $('hFiado').textContent = 'A receber (fiado) · ' + (fi.total||'R$ 0,00');
      rows($('fiado'), fi.clientes, c => `<div class="row"><div class="n">${c.nome}</div><div class="val">${c.saldo}</div></div>`);
    })();

    // Contas a pagar.
    (function(){
      const ap = D.aPagar||{contas:[]};
      $('hPagar').textContent = 'A pagar · ' + (ap.total||'R$ 0,00');
      rows($('pagar'), ap.contas, c => `<div class="row"><div><div class="n">${c.descricao}</div><div class="s" ${c.vencida?'style="color:var(--red)"':''}>${c.fornecedor}${c.vencimento?(' · vence '+c.vencimento):''}${c.vencida?' · VENCIDA':''}</div></div><div class="val">${c.valor}</div></div>`);
    })();

    // Últimas compras.
    rows($('compras'), D.compras, c => `<div class="row"><div><div class="n">${c.fornecedor}</div><div class="s">${fmtData(c.data)}${c.nota?(' · NF '+c.nota):''} · ${c.itens} item(ns)</div></div><div class="val">${c.total}</div></div>`);

    // Produtos parados.
    rows($('parados'), D.parados, p => `<div class="row"><div class="n">${p.nome}</div><div class="val">${p.estoque}</div></div>`);

    document.querySelectorAll('.tab').forEach(t => t.addEventListener('click', () => {
      document.querySelectorAll('.tab').forEach(x=>x.classList.remove('on'));
      t.classList.add('on'); periodo = t.getAttribute('data-p'); renderPeriodo();
    }));
    renderPeriodo();
  </script>
</body>
</html>
)HTMLDOC");

    QString html = kTemplate;
    html.replace(QStringLiteral("%DADOS%"), json);
    return html;
}
