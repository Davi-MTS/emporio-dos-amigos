#include <QtTest>

#include "utils/Money.h"

class TstMoney : public QObject
{
    Q_OBJECT

private slots:
    void formatPlain_data();
    void formatPlain();
    void format_comSimbolo();
    void parse_validos_data();
    void parse_validos();
    void parse_invalidos_data();
    void parse_invalidos();
    void roundtrip();
};

void TstMoney::formatPlain_data()
{
    QTest::addColumn<qint64>("centavos");
    QTest::addColumn<QString>("esperado");

    QTest::newRow("zero")      << qint64(0)         << QStringLiteral("0,00");
    QTest::newRow("5c")        << qint64(5)         << QStringLiteral("0,05");
    QTest::newRow("50c")       << qint64(50)        << QStringLiteral("0,50");
    QTest::newRow("12,50")     << qint64(1250)      << QStringLiteral("12,50");
    QTest::newRow("mil")       << qint64(100000)    << QStringLiteral("1.000,00");
    QTest::newRow("milhoes")   << qint64(123456789) << QStringLiteral("1.234.567,89");
    QTest::newRow("negativo")  << qint64(-1250)     << QStringLiteral("-12,50");
}

void TstMoney::formatPlain()
{
    QFETCH(qint64, centavos);
    QFETCH(QString, esperado);
    QCOMPARE(Money::formatPlain(centavos), esperado);
}

void TstMoney::format_comSimbolo()
{
    QCOMPARE(Money::format(1250), QStringLiteral("R$ 12,50"));
    QCOMPARE(Money::format(0), QStringLiteral("R$ 0,00"));
}

void TstMoney::parse_validos_data()
{
    QTest::addColumn<QString>("texto");
    QTest::addColumn<qint64>("esperado");

    QTest::newRow("virgula")     << QStringLiteral("12,50")     << qint64(1250);
    QTest::newRow("ponto")       << QStringLiteral("12.50")     << qint64(1250);
    QTest::newRow("com_simbolo") << QStringLiteral("R$ 12,50")  << qint64(1250);
    QTest::newRow("milhar")      << QStringLiteral("1.234,56")  << qint64(123456);
    QTest::newRow("so_inteiro")  << QStringLiteral("1000")      << qint64(100000);
    QTest::newRow("uma_casa")    << QStringLiteral("12,5")      << qint64(1250);
    QTest::newRow("centavos")    << QStringLiteral("0,05")      << qint64(5);
    QTest::newRow("negativo")    << QStringLiteral("-12,50")    << qint64(-1250);
}

void TstMoney::parse_validos()
{
    QFETCH(QString, texto);
    QFETCH(qint64, esperado);
    const auto r = Money::parse(texto);
    QVERIFY(r.has_value());
    QCOMPARE(*r, esperado);
}

void TstMoney::parse_invalidos_data()
{
    QTest::addColumn<QString>("texto");

    QTest::newRow("vazio")       << QString();
    QTest::newRow("letras")      << QStringLiteral("abc");
    QTest::newRow("tres_casas")  << QStringLiteral("12,555");
    QTest::newRow("so_traco")    << QStringLiteral("-");
}

void TstMoney::parse_invalidos()
{
    QFETCH(QString, texto);
    QVERIFY(!Money::parse(texto).has_value());
}

void TstMoney::roundtrip()
{
    const QList<qint64> valores = {0, 5, 99, 1250, 100000, 123456789, -1250};
    for (qint64 v : valores) {
        const auto r = Money::parse(Money::formatPlain(v));
        QVERIFY(r.has_value());
        QCOMPARE(*r, v);
    }
}

QTEST_APPLESS_MAIN(TstMoney)
#include "tst_money.moc"
