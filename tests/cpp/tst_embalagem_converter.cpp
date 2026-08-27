#include <QtTest>

#include "domain/produtos/EmbalagemConverter.h"

using namespace EmbalagemConverter;

class TstEmbalagemConverter : public QObject
{
    Q_OBJECT

private slots:
    void paraUnidadeBase_data();
    void paraUnidadeBase();
    void desmembrar_data();
    void desmembrar();
    void valorDaEmbalagem_basico();
    void fatorInvalido();
    void idaEVolta();
};

void TstEmbalagemConverter::paraUnidadeBase_data()
{
    QTest::addColumn<qint64>("qtdEmbalagem");
    QTest::addColumn<int>("fator");
    QTest::addColumn<qint64>("esperado");

    QTest::newRow("3 caixas de 12") << qint64(3)  << 12 << qint64(36);
    QTest::newRow("1 unidade")      << qint64(1)  << 1  << qint64(1);
    QTest::newRow("fardo de 6")     << qint64(10) << 6  << qint64(60);
    QTest::newRow("zero")           << qint64(0)  << 12 << qint64(0);
}

void TstEmbalagemConverter::paraUnidadeBase()
{
    QFETCH(qint64, qtdEmbalagem);
    QFETCH(int, fator);
    QFETCH(qint64, esperado);
    QCOMPARE(EmbalagemConverter::paraUnidadeBase(qtdEmbalagem, fator), esperado);
}

void TstEmbalagemConverter::desmembrar_data()
{
    QTest::addColumn<qint64>("qtdBase");
    QTest::addColumn<int>("fator");
    QTest::addColumn<qint64>("fechadas");
    QTest::addColumn<qint64>("sobra");

    QTest::newRow("37/12")   << qint64(37) << 12 << qint64(3) << qint64(1);
    QTest::newRow("36/12")   << qint64(36) << 12 << qint64(3) << qint64(0);
    QTest::newRow("5/12")    << qint64(5)  << 12 << qint64(0) << qint64(5);
    QTest::newRow("fator 1") << qint64(9)  << 1  << qint64(9) << qint64(0);
}

void TstEmbalagemConverter::desmembrar()
{
    QFETCH(qint64, qtdBase);
    QFETCH(int, fator);
    QFETCH(qint64, fechadas);
    QFETCH(qint64, sobra);
    const Desmembramento d = EmbalagemConverter::desmembrar(qtdBase, fator);
    QCOMPARE(d.embalagensFechadas, fechadas);
    QCOMPARE(d.sobra, sobra);
}

void TstEmbalagemConverter::valorDaEmbalagem_basico()
{
    // Custo base 250 centavos, caixa de 12 -> 3000 centavos.
    QCOMPARE(valorDaEmbalagem(250, 12), qint64(3000));
    QCOMPARE(valorDaEmbalagem(0, 12), qint64(0));
}

void TstEmbalagemConverter::fatorInvalido()
{
    // Fator inválido nunca deve "explodir": retorna valores seguros.
    QCOMPARE(EmbalagemConverter::paraUnidadeBase(5, 0), qint64(0));
    QCOMPARE(EmbalagemConverter::paraUnidadeBase(5, -3), qint64(0));
    QCOMPARE(EmbalagemConverter::valorDaEmbalagem(100, 0), qint64(0));

    const Desmembramento d = EmbalagemConverter::desmembrar(42, 0);
    QCOMPARE(d.embalagensFechadas, qint64(0));
    QCOMPARE(d.sobra, qint64(42));
}

void TstEmbalagemConverter::idaEVolta()
{
    // Converter para base e desmembrar de volta reconstrói a quantidade.
    const int fator = 12;
    for (qint64 caixas = 0; caixas < 100; ++caixas) {
        const qint64 base = EmbalagemConverter::paraUnidadeBase(caixas, fator);
        const Desmembramento d = EmbalagemConverter::desmembrar(base, fator);
        QCOMPARE(d.embalagensFechadas, caixas);
        QCOMPARE(d.sobra, qint64(0));
    }
}

QTEST_APPLESS_MAIN(TstEmbalagemConverter)
#include "tst_embalagem_converter.moc"
