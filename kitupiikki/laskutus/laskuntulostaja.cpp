/*
   Copyright (C) 2017 Arto Hyvättinen

   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program. If not, see <http://www.gnu.org/licenses/>.
*/

#include <QDebug>
#include <cmath>
#include <QSvgRenderer>
#include <QPdfWriter>

#include <QApplication>

#include "laskuntulostaja.h"
#include "db/kirjanpito.h"
#include "laskumodel.h"
#include "nayukiQR/QrCode.hpp"



LaskunTulostaja::LaskunTulostaja(LaskuModel *model) : QObject(model), model_(model)
{
    // Hakee laskulle tulostuvan IBAN-numeron
    iban = kp()->tilit()->tiliNumerolla( kp()->asetukset()->luku("LaskuTili")).json()->str("IBAN");
}

bool LaskunTulostaja::tulosta(QPagedPaintDevice *printer, QPainter *painter)
{
    double mm = printer->width() * 1.00 / printer->widthMM();
    qreal marginaali = 0.0;

    if( kp()->asetukset()->onko("Harjoitus") && !kp()->asetukset()->onko("Demo") )
    {
        painter->save();
        painter->setPen( QPen(Qt::green));
        painter->setFont( QFont("Sans",14));
        painter->drawText(QRect( 0, 0, painter->window().width(), painter->window().height() ), Qt::AlignTop | Qt::AlignRight, QString("HARJOITUS") );
        painter->restore();
    }

    if( model_->laskunSumma() > 0 && model_->kirjausperuste() != LaskuModel::KATEISLASKU)
    {
        painter->translate( 0, painter->window().height() - mm * 95 );
        marginaali += alatunniste(printer, painter) + mm * 95;
        tilisiirto(printer, painter);
        painter->resetTransform();
    }
    else
    {
        painter->translate(0, painter->window().height());
        marginaali += alatunniste(printer, painter);
    }
    painter->resetTransform();

    ylaruudukko(printer, painter);
    lisatieto(painter, model_->lisatieto());
    erittely( model_, printer, painter, marginaali);

    painter->translate(0, mm*15);

    if( model_->tyyppi() == LaskuModel::MAKSUMUISTUTUS)
    {
        lisatieto(painter, tr("Alkuperäinen lasku\n"
                               "Laskun numero: %1 \t Laskun päiväys: %2 \t Eräpäivä: %3")
                  .arg( model_->viittausLasku().viite)
                  .arg( model_->viittausLasku().pvm.toString("dd.MM.yyyy"))
                  .arg( model_->viittausLasku().erapvm.toString("dd.MM.yyyy")));

        LaskuModel *alkuperainenLasku = LaskuModel::haeLasku( model_->viittausLasku().eraId );
        erittely( alkuperainenLasku, printer, painter, marginaali);
    }

    painter->resetTransform();

    return true;
}

QByteArray LaskunTulostaja::pdf()
{
    QByteArray array;
    QBuffer buffer(&array);
    buffer.open(QIODevice::WriteOnly);

    QPdfWriter writer(&buffer);
    QPainter painter(&writer);

    writer.setCreator(QString("%1 %2").arg( qApp->applicationName() ).arg( qApp->applicationVersion() ));
    writer.setTitle( tr("Lasku %1").arg(model_->laskunro()));
    tulosta(&writer, &painter);
    painter.end();

    buffer.close();

    return array;
}

QString LaskunTulostaja::html()
{
    QString txt = "<html><body><table width=100%>\n";


    QString osoite = model_->osoite();
    osoite.replace("\n","<br/>");

    QString omaosoite = kp()->asetukset()->asetus("Osoite");
    omaosoite.replace("\n","<br>");

    QString otsikko = tr("Lasku");
    if( model_->tyyppi() == LaskuModel::HYVITYSLASKU)
        otsikko = tr("Hyvityslasku laskulle %1").arg( model_->viittausLasku().viite);
    else if( model_->tyyppi() == LaskuModel::MAKSUMUISTUTUS)
        otsikko = tr("MAKSUMUISTUTUS");
    else if(model_->kirjausperuste() == LaskuModel::KATEISLASKU)
        otsikko = tr("Kuitti");

    if( kp()->asetukset()->onko("Harjoitus") && !kp()->asetukset()->onko("Demo") )
    {
        otsikko = QString("<p style='text-align:right;'><span style='color: green;'>HARJOITUS</span></p>") + otsikko;
    }

    txt.append(tr("<tr><td width=50% style=\"border-bottom: 1px solid black;\">%1<br>%2</td><td colspan=2 style='font-size: large; border-bottom: 1px solid black;'>%3</td></tr>\n").arg(kp()->asetukset()->asetus("Nimi")).arg(omaosoite).arg(otsikko) );
    txt.append(QString("<td rowspan=7 style=\"border-bottom: 1px solid black; border-right: 1px solid black;\">%1</td>").arg(osoite));

    if( model_->tyyppi() != LaskuModel::HYVITYSLASKU )
        txt.append(tr("<td width=25% style=\"border-bottom: 1px solid black;\">Laskun päivämäärä</td><td width=25% style=\"border-bottom: 1px solid black;\">%1</td></tr>\n").arg( kp()->paivamaara().toString("dd.MM.yyyy") ));
    else
        txt.append(tr("<td width=25% style=\"border-bottom: 1px solid black;\">Hyvityksen päivämäärä</td><td width=25% style=\"border-bottom: 1px solid black;\">%1</td></tr>\n").arg( kp()->paivamaara().toString("dd.MM.yyyy") ));

    if( model_->kirjausperuste() == LaskuModel::KATEISLASKU)
        txt.append(tr("<tr><td style=\"border-bottom: 1px solid black;\">Laskun numero</td><td style=\"border-bottom: 1px solid black;\">%1</td></tr>\n").arg(model_->viitenumero() ));
    else if( model_->tyyppi() == LaskuModel::HYVITYSLASKU)
        txt.append(tr("<tr><td style=\"border-bottom: 1px solid black;\">Hyvityslaskun numero</td><td style=\"border-bottom: 1px solid black;\">%1</td></tr>\n").arg(model_->viitenumero() ));
    else
        txt.append(tr("<tr><td style=\"border-bottom: 1px solid black;\">Viitenumero</td><td style=\"border-bottom: 1px solid black;\">%1</td></tr>\n").arg(model_->viitenumero() ));

    // Käteislaskulla tai hyvityslaskulla ei eräpäivää
    if( model_->kirjausperuste() != LaskuModel::KATEISLASKU && model_->tyyppi() != LaskuModel::HYVITYSLASKU)
        txt.append(tr("<tr><td style=\"border-bottom: 1px solid black;\">Eräpäivä</td><td style=\"border-bottom: 1px solid black;\">%1</td></tr>\n").arg(model_->erapaiva().toString("dd.MM.yyyy")));

    txt.append(tr("<tr><td style=\"border-bottom: 1px solid black;\">Summa</td><td style=\"border-bottom: 1px solid black;\">%L1 €</td>\n").arg( (model_->laskunSumma() / 100.0) ,0,'f',2));

    if( model_->kirjausperuste() != LaskuModel::KATEISLASKU && model_->tyyppi() != LaskuModel::HYVITYSLASKU)
    {
        txt.append(tr("<tr><td style=\"border-bottom: 1px solid black;\">IBAN</td><td style=\"border-bottom: 1px solid black;\">%1</td></tr>").arg( iban) );
    }

    if( !model_->ytunnus().isEmpty())
        txt.append(tr("<tr><td style=\"border-bottom: 1px solid black;\">Asiakkaan Y-tunnus</td><td style=\"border-bottom: 1px solid black;\">%1</td></tr>\n").arg( model_->ytunnus() ));
    if( !model_->asiakkaanViite().isEmpty())
        txt.append(tr("<tr><td style=\"border-bottom: 1px solid black;\">Asiakkaan Y-tunnus</td><td style=\"border-bottom: 1px solid black;\">%1</td></tr>\n").arg( model_->asiakkaanViite() ));



    QString lisatieto = model_->lisatieto();
    lisatieto.replace("\n","<br>");

    txt.append("</table><p>" + lisatieto + "</p><table width=100% style=\"margin-top: 2em; margin-bottom: 1em\">");

    bool alv = kp()->asetukset()->onko("AlvVelvollinen");

    if(alv)
        txt.append(tr("<tr><th style=\"border-bottom: 1px solid black;\">Nimike</th><th style=\"border-bottom: 1px solid black;\">Määrä</th><th style=\"border-bottom: 1px solid black;\">"
                      "á netto</th><th style=\"border-bottom: 1px solid black;\">Alv %</th><th style=\"border-bottom: 1px solid black;\">Alv</th><th style=\"border-bottom: 1px solid black;\">Yhteensä</th></tr>\n"));
    else
        txt.append(tr("<tr><th style=\"border-bottom: 1px solid black;\">Nimike</th><th style=\"border-bottom: 1px solid black;\">Määrä</th><th style=\"border-bottom: 1px solid black;\">á netto</th><th style=\"border-bottom: 1px solid black;\">Yhteensä</th></tr>\n"));


    qreal kokoNetto = 0.0;
    qreal kokoVero = 0.0;

    QMap<int,AlvErittelyEra> alvit;

    for(int i=0; i < model_->rowCount(QModelIndex());i++)
    {
        int yhtsnt = model_->data( model_->index(i, LaskuModel::BRUTTOSUMMA), Qt::EditRole ).toInt();
        if( !yhtsnt)
            continue;   // Ohitetaan tyhjät rivit

        QString nimike = model_->data( model_->index(i, LaskuModel::NIMIKE), Qt::DisplayRole ).toString();
        QString maara = model_->data( model_->index(i, LaskuModel::MAARA), Qt::DisplayRole ).toString();
        QString yksikko = model_->data( model_->index(i, LaskuModel::YKSIKKO), Qt::DisplayRole ).toString();
        QString ahinta = model_->data( model_->index(i, LaskuModel::AHINTA), Qt::DisplayRole ).toString();
        QString vero = model_->data( model_->index(i, LaskuModel::ALV), Qt::DisplayRole ).toString();
        QString yht = model_->data( model_->index(i, LaskuModel::BRUTTOSUMMA), Qt::DisplayRole ).toString();

        double verosnt = model_->data( model_->index(i, LaskuModel::NIMIKE), LaskuModel::VeroRooli ).toDouble();
        double nettosnt = model_->data( model_->index(i, LaskuModel::NIMIKE), LaskuModel::NettoRooli ).toDouble();
        int veroprossa = model_->data( model_->index(i, LaskuModel::NIMIKE), LaskuModel::AlvProsenttiRooli ).toInt();

        kokoNetto += nettosnt;

        if( alv )
        {
            txt.append(QString("<tr><td>%1</td><td style='text-align:right;'>%2 %3</td><td style='text-align:right;'>%4</td><td style='text-align:right;'>%5</td><td style='text-align:right;'>%L6 €</td><td style='text-align:right;'>%7</td></tr>\n")
                       .arg(nimike).arg(maara).arg(yksikko).arg(ahinta).arg(vero).arg(verosnt / 100.0,0,'f',2).arg(yht) );

            // Lisätään alv-erittelyä varten summataulukkoihin
            if( !alvit.contains(veroprossa))
                alvit.insert(veroprossa, AlvErittelyEra() );

            alvit[veroprossa].netto += nettosnt;
            alvit[veroprossa].vero += verosnt;
            kokoVero += verosnt;
        }
        else
        {
            txt.append(QString("<tr><td>%1</td><td style='text-align:right;'>%2 %3</td><td style='text-align:right;'>%4</td><td style='text-align:right;'>%5</td></tr>\n")
                       .arg(nimike).arg(maara).arg(yksikko).arg(ahinta).arg(yht) );            
        }
    }
    txt.append("</table><p>");
    if( alv && model_->tyyppi() != LaskuModel::MAKSUMUISTUTUS)
    {
        txt.append(tr("<table width=50% style=\"margin-left: auto;\"><tr><th style=\"border-bottom: 1px solid black;\">Alv%</th><th style=\"border-bottom: 1px solid black;\">Veroton</th><th style=\"border-bottom: 1px solid black;\">Vero</th><th style=\"border-bottom: 1px solid black;\">Yhteensä</th></tr>"));
        QMapIterator<int,AlvErittelyEra> iter(alvit);
        while(iter.hasNext())
        {
            iter.next();
            txt.append( tr("<tr><td>%1 %</td><td style='text-align:right;'>%L2 €</td><td style='text-align:right;'>%L3 €</td><td  style='text-align:right;'>%L4 €</td><tr>\n")
                        .arg( iter.key())
                        .arg( ( iter.value().netto / 100.0) ,0,'f',2)
                        .arg( ( iter.value().vero / 100.0) ,0,'f',2)
                        .arg( (iter.value().brutto() / 100.0) ,0,'f',2) ) ;
        }
        txt.append( tr("<tr><td><b>Yhteensä</b> </td><td td style='text-align:right;'><b>%L1 €</b></td><td style='text-align:right;'><b>%L2 €</b></td><td style='text-align:right;'><b>%L3 €</b></td><tr>\n")
                    .arg( ( kokoNetto / 100.0) ,0,'f',2)
                    .arg( ( kokoVero / 100.0) ,0,'f',2)
                    .arg( ( model_->laskunSumma() / 100.0) ,0,'f',2) ) ;
        txt.append("</table>");

    }

    if( virtuaaliviivakoodi().length() > 50)
        txt.append(tr("<hr><p>Virtuaaliviivakoodi <b>%1</b></p><hr>").arg( virtuaaliviivakoodi()) );

    txt.append("<table>");
    if( kp()->asetukset()->asetus("Ytunnus").length())
        txt.append(tr("<tr><td>Y-tunnus </td><td>%1</td></tr>\n").arg(kp()->asetukset()->asetus("Ytunnus")));
    if( kp()->asetukset()->asetus("Puhelin").length())
        txt.append(tr("<tr><td>Puhelin </td><td>%2</td></tr> \n").arg(kp()->asetukset()->asetus("Puhelin")));
    if( kp()->asetukset()->asetus("Sahkoposti").length())
        txt.append(tr("<tr><td>Sähköposti </td><td>%2</td></tr> \n").arg(kp()->asetukset()->asetus("Sahkoposti")));

    txt.append("</table></body></html>\n");

    return txt;
}

QString LaskunTulostaja::virtuaaliviivakoodi() const
{
    if( model_->laskunSumma() > 99999999 )  // Ylisuuri laskunsumma
        return QString();

    QString koodi = QString("4 %1 %2 000 %3 %4")
            .arg( iban.mid(2,16) )  // Tilinumeron numeerinen osuus
            .arg( model_->laskunSumma(), 8, 10, QChar('0') )  // Rahamäärä
            .arg( model_->viitenumero().remove(QChar(' ')), 20, QChar('0'))
            .arg( model_->erapaiva().toString("yyMMdd"));

    return koodi.remove(QChar(' '));
}

QString LaskunTulostaja::valeilla(const QString &teksti)
{
    QString palautettava;
    for(int i=0; i < teksti.length(); i++)
    {
        palautettava.append(teksti.at(i));
        if( i % 4 == 3)
            palautettava.append(QChar(' '));
    }
    return palautettava;
}

void LaskunTulostaja::ylaruudukko(QPagedPaintDevice *printer, QPainter *painter)
{
    const int TEKSTIPT = 10;
    const int OTSAKEPT = 7;

    double mm = printer->width() * 1.00 / printer->widthMM();

    // Lasketaan rivinkorkeus. Tehdään painterin kautta, jotta toimii myös pdf-writerillä
    painter->setFont( QFont("Sans",OTSAKEPT) );
    double rk = painter->fontMetrics().height();
    painter->setFont(QFont("Sans",TEKSTIPT));
    rk += painter->fontMetrics().height();
    rk += 2 * mm;

    double leveys = painter->window().width();

    // Kuoren ikkuna
    QRectF ikkuna;
    double keskiviiva = leveys / 2;

    if( kp()->asetukset()->onko("LaskuIkkuna"))
        ikkuna = QRectF( (kp()->asetukset()->luku("LaskuIkkunaX", 0) - printer->pageLayout().margins(QPageLayout::Millimeter).left()  ) * mm,
                       (kp()->asetukset()->luku("LaskuIkkunaY",0) - printer->pageLayout().margins(QPageLayout::Millimeter).top()) * mm,
                       kp()->asetukset()->luku("LaskuIkkunaLeveys",90) * mm, kp()->asetukset()->luku("LaskuIkkunaKorkeus",30) * mm);
    else
        ikkuna = QRectF( 0, rk * 3, keskiviiva, rk * 3);



    if( ikkuna.x() + ikkuna.width() > keskiviiva )
            keskiviiva = ikkuna.x() + ikkuna.width() + 2 * mm;

    double puoliviiva = keskiviiva + ( leveys - keskiviiva ) / 2;

    QRectF lahettajaAlue = QRectF( 0, 0, keskiviiva, rk * 2.2);

    // Jos käytössä on isoikkunakuori, tulostetaan myös lähettäjän nimi ja osoite sinne
    if( kp()->asetukset()->luku("LaskuIkkunaKorkeus", 35) > 55 )
    {
        lahettajaAlue = QRectF( ikkuna.x(), ikkuna.y(), ikkuna.width(), 30 * mm);
        ikkuna = QRectF( ikkuna.x(), ikkuna.y() + 30 * mm, ikkuna.width(), ikkuna.height() - 30 * mm);
    }


    // Lähettäjätiedot

    double vasen = 0.0;
    if( !kp()->logo().isNull() )
    {
        double logosuhde = (1.0 * kp()->logo().width() ) / kp()->logo().height();
        double skaala = logosuhde < 5.00 ? logosuhde : 5.00;    // Logon sallittu suhde enintään 5:1

        painter->drawImage( QRectF( lahettajaAlue.x()+mm, lahettajaAlue.y()+mm, rk*2*skaala, rk*2 ),  kp()->logo()  );
        vasen += rk * 2.2 * skaala;

    }
    painter->setFont(QFont("Sans",14));
    double pv = painter->fontMetrics().height();
    QString nimi = kp()->asetukset()->onko("LogossaNimi") ? QString() : kp()->asetus("Nimi");   // Jos nimi logossa, sitä ei toisteta
    QRectF lahettajaRect = painter->boundingRect( QRectF( lahettajaAlue.x()+vasen, lahettajaAlue.y(),
                                                       lahettajaAlue.width()-vasen, 20 * mm), Qt::TextWordWrap, nimi );
    painter->drawText(QRectF( lahettajaRect), Qt::AlignLeft | Qt::TextWordWrap, nimi);

    painter->setFont(QFont("Sans",9));
    QRectF lahettajaosoiteRect = painter->boundingRect( QRectF( lahettajaAlue.x()+vasen, lahettajaAlue.y() + lahettajaRect.height(),
                                                       lahettajaAlue.width()-vasen, 20 * mm), Qt::TextWordWrap, kp()->asetus("Osoite") );
    painter->drawText(lahettajaosoiteRect, Qt::AlignLeft, kp()->asetus("Osoite") );

    // Tulostetaan saajan osoite ikkunaan
    painter->setFont(QFont("Sans", TEKSTIPT));
    painter->drawText(ikkuna, Qt::TextWordWrap, model_->osoite());

    pv += rk ;     // pv = perusviiva

    painter->setPen( QPen( QBrush(Qt::black), 0.13));
    painter->drawLine( QLineF(keskiviiva, pv, leveys, pv ));
    painter->drawLine( QLineF(keskiviiva, pv-rk, leveys, pv-rk ));
    painter->drawLine( QLineF(keskiviiva, pv-rk, keskiviiva, pv+rk*5));
    if( !model_->ytunnus().isEmpty())
        painter->drawLine( QLineF(puoliviiva, pv, puoliviiva, pv+rk ));

    painter->drawLine( QLineF(keskiviiva, pv+ rk*4, leveys, pv + rk*4));
    painter->drawLine(QLineF(puoliviiva, pv+rk*2, puoliviiva, pv+rk*4));
    for(int i=1; i<6; i++)
        painter->drawLine(QLineF(keskiviiva, pv + i * rk, leveys, pv + i * rk));

    painter->setFont( QFont("Sans",OTSAKEPT) );
    if( model_->kirjausperuste() != LaskuModel::MAKSUPERUSTE)
    {
        painter->drawLine(QLineF(puoliviiva, pv-rk, puoliviiva, pv));
        painter->drawText(QRectF( puoliviiva + mm, pv - rk + mm, leveys / 4, rk ), Qt::AlignTop, tr("Kirjanpidon tositenro"));
    }

    painter->drawText(QRectF( keskiviiva + mm, pv - rk + mm, leveys / 4, rk ), Qt::AlignTop, tr("Päivämäärä"));

    if( model_->tyyppi() == LaskuModel::HYVITYSLASKU )
        painter->drawText(QRectF( keskiviiva + mm, pv + mm, leveys / 4, rk ), Qt::AlignTop, tr("Hyvityksen päivämäärä"));
    else
        painter->drawText(QRectF( keskiviiva + mm, pv + mm, leveys / 4, rk ), Qt::AlignTop, tr("Toimituspäivä"));

    if( !model_->ytunnus().isEmpty())
        painter->drawText(QRectF( puoliviiva + mm, pv + mm, leveys / 4, rk), Qt::AlignTop, tr("Asiakkaan Y-tunnus"));

    if( model_->kirjausperuste() == LaskuModel::KATEISLASKU)
        painter->drawText(QRectF( keskiviiva + mm, pv + rk + mm, leveys / 4, rk ), Qt::AlignTop, tr("Laskun numero"));
    else if( model_->tyyppi() == LaskuModel::HYVITYSLASKU)
        painter->drawText(QRectF( keskiviiva + mm, pv + rk + mm, leveys / 4, rk ), Qt::AlignTop, tr("Hyvityslaskun numero"));
    else
        painter->drawText(QRectF( keskiviiva + mm, pv + rk + mm, leveys / 4, rk ), Qt::AlignTop, tr("Viitenumero"));


    painter->drawText(QRectF( keskiviiva + mm, pv + rk * 2 + mm, leveys / 4, rk ), Qt::AlignTop, tr("Eräpäivä"));
    painter->drawText(QRectF( puoliviiva + mm, pv + rk * 2 + mm, leveys / 4, rk ), Qt::AlignTop, tr("Summa"));
    painter->drawText(QRectF( keskiviiva + mm, pv + rk * 3 + mm, leveys / 4, rk ), Qt::AlignTop, tr("Huomautusaika"));
    painter->drawText(QRectF( puoliviiva + mm, pv + rk * 3 + mm, leveys / 4, rk ), Qt::AlignTop, tr("Viivästyskorko"));
    painter->drawText(QRectF( keskiviiva + mm, pv + rk * 4 + mm, leveys / 4, rk ), Qt::AlignTop, tr("Asiakkaan viite"));

    painter->setFont(QFont("Sans", TEKSTIPT));

    // Haetaan tositetunniste
    if( model_->kirjausperuste() != LaskuModel::MAKSUPERUSTE)
    {
        painter->drawText(QRectF( puoliviiva + mm, pv - rk, leveys / 4, rk-mm ), Qt::AlignBottom, model_->tositetunnus() );
    }

    painter->drawText(QRectF( keskiviiva + mm, pv - rk, leveys / 4, rk-mm ), Qt::AlignBottom, kp()->paivamaara().toString("dd.MM.yyyy") );
    painter->drawText(QRectF( keskiviiva + mm, pv + mm, leveys / 4, rk-mm ), Qt::AlignBottom,  model_->toimituspaiva().toString("dd.MM.yyyy") );

    if( !model_->ytunnus().isEmpty())
        painter->drawText(QRectF( puoliviiva + mm, pv + mm, leveys / 4, rk-mm ), Qt::AlignBottom,  model_->ytunnus() );


    painter->drawText(QRectF( keskiviiva + mm, pv+rk+mm, leveys / 2, rk-mm ), Qt::AlignBottom, model_->viitenumero() );



    if( model_->kirjausperuste() == LaskuModel::KATEISLASKU)
    {
        painter->drawText(QRectF( keskiviiva + mm, pv - rk * 2, leveys / 4, rk-mm ), Qt::AlignBottom,  tr("Käteislasku / Kuitti") );
        painter->drawText(QRectF( keskiviiva + mm, pv + rk * 2, leveys / 4, rk-mm ), Qt::AlignBottom,  tr("Maksettu") );
    }
    else if( model_->tyyppi() == LaskuModel::HYVITYSLASKU)
    {
        painter->drawText(QRectF( keskiviiva + mm, pv - rk * 2, leveys - keskiviiva, rk-mm ), Qt::AlignBottom,  tr("Hyvityslasku laskulle %1")
                          .arg( model_->viittausLasku().viite ));
    }
    else
    {
        if( model_->tyyppi() == LaskuModel::MAKSUMUISTUTUS)
        {
            painter->setFont( QFont("Sans", TEKSTIPT+2,QFont::Black));
            painter->drawText(QRectF( keskiviiva + mm, pv - rk * 2, leveys / 4, rk-mm ), Qt::AlignBottom,  tr("MAKSUMUISTUTUS") );
            painter->setFont(QFont("Sans", TEKSTIPT));
        }
        else
            painter->drawText(QRectF( keskiviiva + mm, pv - rk * 2, leveys / 4, rk-mm ), Qt::AlignBottom,  tr("Lasku") );

        if( model_->laskunSumma() > 0.0 )  // Näytetään eräpäivä vain jos on maksettavaa
        {
            painter->drawText(QRectF( keskiviiva + mm, pv + rk * 2, leveys / 4, rk-mm ), Qt::AlignBottom,  model_->erapaiva().toString("dd.MM.yyyy") );
            painter->drawText(QRectF( puoliviiva + mm, pv + rk * 3, leveys / 4, rk-mm ), Qt::AlignBottom,  kp()->asetus("LaskuViivastyskorko") );
        }

    }

    painter->drawText(QRectF( puoliviiva + mm, pv + rk * 2, leveys / 4, rk-mm ), Qt::AlignBottom,  QString("%L1 €").arg( (model_->laskunSumma() / 100.0) ,0,'f',2));
    painter->drawText(QRectF( keskiviiva + mm, pv + rk * 3, leveys / 4, rk-mm ), Qt::AlignBottom,  kp()->asetus("LaskuHuomautusaika") );
    painter->drawText(QRectF( keskiviiva + mm, pv + rk * 4, leveys / 2, rk-mm ), Qt::AlignBottom,  model_->asiakkaanViite() );

    // Kirjoittamista jatkettaan ruudukon jälkeen - taikka ikkunan, jos se on isompi

    if( ikkuna.y() + ikkuna.height() > pv + 5*rk)
        painter->translate( 0,  ikkuna.y() + ikkuna.height() + 5 * mm);
    else
        painter->translate(0, pv + 5*rk + 5 * mm);
}

void LaskunTulostaja::lisatieto(QPainter *painter, const QString& lisatieto)
{
    painter->setFont( QFont("Sans", 10));
    QRectF ltRect = painter->boundingRect(QRect(0,0,painter->window().width(), painter->window().height()), Qt::TextWordWrap, lisatieto );
    painter->drawText(ltRect, Qt::TextWordWrap, lisatieto );
    if( ltRect.height() > 0 )
        painter->translate( 0, ltRect.height() + painter->fontMetrics().height());  // Vähän väliä


}

qreal LaskunTulostaja::alatunniste(QPagedPaintDevice *printer, QPainter *painter)
{
    painter->setFont( QFont("Sans",10));
    qreal rk = painter->fontMetrics().height();
    painter->save();
    painter->translate(0, -2.5 * rk);

    qreal leveys = painter->window().width();
    double mm = printer->width() * 1.00 / printer->widthMM();

    if( !kp()->asetukset()->asetus("Puhelin").isEmpty() )
        painter->drawText(QRectF(0,0,leveys/3,rk), Qt::AlignLeft, tr("Puh. %1").arg(kp()->asetus("Puhelin")));
    if( !kp()->asetukset()->asetus("Sahkoposti").isEmpty() )
        painter->drawText(QRectF(0,rk,leveys/3,rk), Qt::AlignLeft, tr("Sähköposti %1").arg(kp()->asetus("Sahkoposti")));

    painter->drawText(QRectF(leveys / 3,0,leveys/3,rk), Qt::AlignCenter, tr("IBAN %1").arg( valeilla( iban ) ));
    painter->drawText(QRectF(2 *leveys / 3,0,leveys/3,rk), Qt::AlignRight, tr("Y-tunnus %1").arg(kp()->asetus("Ytunnus")));
    painter->setPen( QPen(QBrush(Qt::black), mm * 0.13));
    painter->drawLine( QLineF( 0, 0, leveys, 0));

    bool rakennuspalvelut = false;
    bool yhteisomyynti = false;

    // Tarkistetaan verotyypit ja niiden mukaiset tekstit
    for(int i=0; i<model_->rowCount(QModelIndex()); i++)
    {
        int verokoodi = model_->data( model_->index(i,0), LaskuModel::AlvKoodiRooli ).toInt();
        if( verokoodi == AlvKoodi::RAKENNUSPALVELU_MYYNTI)
            rakennuspalvelut = true;
        else if( verokoodi == AlvKoodi::YHTEISOMYYNTI_TAVARAT || verokoodi == AlvKoodi::YHTEISOMYYNTI_PALVELUT)
            yhteisomyynti = true;
    }
    painter->translate(0, -2 * rk);

    if( yhteisomyynti)
    {
        painter->drawText(QRectF(0,0,leveys,rk), tr("AVL 72 a §: ALV 0% Yhteisömyynti  / VAT 0% Intra Community supply"));
        painter->translate(0, -1 * rk);
    }
    if( rakennuspalvelut)
        painter->drawText(QRectF(0,0,leveys,rk), tr("AVL 8 c §: Käännetty verovelvollisuus / Reverse charge"));


    qreal tila = 0 - painter->transform().dy();
    painter->restore();
    return tila;
}

void LaskunTulostaja::erittely(LaskuModel *model, QPagedPaintDevice *printer, QPainter *painter, qreal marginaali)
{
    bool alv = kp()->asetukset()->onko("AlvVelvollinen");
    erittelyOtsikko(printer, painter, alv);
    double mm = printer->width() * 1.00 / printer->widthMM();

    painter->setFont( QFont("Sans",10));
    qreal leveys = painter->window().width();
    qreal korkeus = painter->window().height() - marginaali;
    qreal rk = painter->fontMetrics().height();
    qreal kokoNetto = 0.0;
    qreal kokoVero = 0.0;

    QMap<int,AlvErittelyEra> alvit;

    for(int i=0; i < model->rowCount(QModelIndex());i++)
    {
        int yhtsnt = model->data( model->index(i, LaskuModel::BRUTTOSUMMA), Qt::EditRole ).toInt();
        if( !yhtsnt)
            continue;   // Ohitetaan tyhjät rivit

        QString nimike = model->data( model->index(i, LaskuModel::NIMIKE), Qt::DisplayRole ).toString();
        QString maara = model->data( model->index(i, LaskuModel::MAARA), Qt::DisplayRole ).toString();
        QString yksikko = model->data( model->index(i, LaskuModel::YKSIKKO), Qt::DisplayRole ).toString();
        QString ahinta = model->data( model->index(i, LaskuModel::AHINTA), Qt::DisplayRole ).toString();
        QString vero = model->data( model->index(i, LaskuModel::ALV), Qt::DisplayRole ).toString();
        QString yht = model->data( model->index(i, LaskuModel::BRUTTOSUMMA), Qt::DisplayRole ).toString();

        double verosnt = model->data( model->index(i, LaskuModel::NIMIKE), LaskuModel::VeroRooli ).toDouble();
        double nettosnt = model->data( model->index(i, LaskuModel::NIMIKE), LaskuModel::NettoRooli ).toDouble();
        int veroprossa = model->data( model->index(i, LaskuModel::NIMIKE), LaskuModel::AlvProsenttiRooli ).toInt();

        // Lisätään alv-erittelyä varten summataulukkoihin
        if( !alvit.contains(veroprossa))
            alvit.insert(veroprossa, AlvErittelyEra() );

        alvit[veroprossa].netto += nettosnt;
        alvit[veroprossa].vero += verosnt;
        kokoNetto += nettosnt;
        kokoVero += verosnt;


        QRectF rect = alv ? painter->boundingRect(QRectF(0,0, 5 * leveys/16, leveys), Qt::TextWordWrap, nimike ) : painter->boundingRect(QRectF(0,0,5*leveys/8, leveys), Qt::TextWordWrap, nimike );

        qreal tamarivi = rect.height() > 0.0 ? rect.height() : rk;
        if( tamarivi + painter->transform().dy() > korkeus - 4 * rk )
        {
            painter->drawText(QRectF(7*leveys/8,0,leveys/8,rk), Qt::AlignLeft, tr("Jatkuu ..."));
            printer->newPage();
            painter->resetTransform();
            korkeus = painter->window().height();
            erittelyOtsikko(printer, painter, alv);
        }
        painter->drawText(rect, Qt::TextWordWrap, nimike);

        if( alv )
        {
            painter->drawText(QRectF(5 *leveys / 16,0,leveys/16-mm,rk), Qt::AlignRight, maara);
            painter->drawText(QRectF(6 *leveys / 16 + mm ,0,leveys/16,rk), Qt::AlignLeft, yksikko);

            painter->drawText(QRectF(7 *leveys / 16,0,leveys/8,rk), Qt::AlignRight, ahinta);

            if( nettosnt > 0.0)
                painter->drawText(QRectF(9 *leveys / 16,0,leveys/8,rk), Qt::AlignRight, QString("%L1 €").arg( (nettosnt / 100.0) ,0,'f',2)  );

            if( vero.endsWith("%"))
            {
                painter->drawText(QRectF(11 *leveys / 16,0,leveys/16,rk), Qt::AlignRight, vero);
                painter->drawText(QRectF(12 *leveys / 16,0,leveys/8,rk), Qt::AlignRight, QString("%L1 €").arg( (verosnt / 100.0) ,0,'f',2) );
            }
            else
                painter->drawText(QRectF(11 *leveys / 16,0,3* leveys/16,rk), Qt::AlignCenter, vero);

            painter->drawText(QRectF(7*leveys/8,0,leveys/8,rk), Qt::AlignRight, yht);
        }
        else
        {
            painter->drawText(QRectF(10 *leveys / 16,0,leveys/16-mm,rk), Qt::AlignRight, maara);
            painter->drawText(QRectF(11 *leveys / 16 + mm ,0,leveys/16,rk), Qt::AlignLeft, yksikko);
            painter->drawText(QRectF(6 *leveys / 8,0,leveys/8,rk), Qt::AlignRight, ahinta);
            painter->drawText(QRectF(7*leveys/8,0,leveys/8,rk), Qt::AlignRight, yht);
        }

        painter->translate(0, tamarivi);
    }

    // ALV-erittelyn tulostus
    if( alv && model->tyyppi() != LaskuModel::MAKSUMUISTUTUS && model->laskunSumma() > 1e-5)
    {
        painter->translate( 0, rk * 0.5);
        painter->drawLine(QLineF(7*leveys / 16.0, 0, leveys, 0));
        painter->drawText(QRectF(7 *leveys / 16,0,leveys/8,rk), Qt::AlignLeft, tr("Alv-erittely")  );
        QMapIterator<int,AlvErittelyEra> iter(alvit);
        while(iter.hasNext())
        {
            iter.next();
            painter->drawText(QRectF(9 *leveys / 16,0,leveys/8,rk), Qt::AlignRight, QString("%L1 €").arg( ( iter.value().netto / 100.0) ,0,'f',2)  );
            painter->drawText(QRectF(11 *leveys / 16,0,leveys/16,rk), Qt::AlignRight, QString("%1 %").arg( iter.key() ) );
            painter->drawText(QRectF(12 *leveys / 16,0,leveys/8,rk), Qt::AlignRight, QString("%L1 €").arg( ( iter.value().vero / 100.0) ,0,'f',2) );
            painter->drawText(QRectF(7*leveys/8,0,leveys/8,rk), Qt::AlignRight, QString("%L1 €").arg( ( iter.value().brutto() / 100.0) ,0,'f',2) );
            painter->translate(0, rk);
        }
    }
    painter->translate(0, rk * 0.25);

    qreal yhtviivaAlkaa = alv == true ? 7 * leveys / 16.0 : 10 * leveys / 16.0; // ilman alviä lyhyempi yhteensä-viiva

    painter->drawLine(QLineF(yhtviivaAlkaa, -0.26 * mm , leveys, -0.26 * mm));
    painter->drawLine(QLineF(yhtviivaAlkaa, 0, leveys, 0));
    painter->drawText(QRectF(yhtviivaAlkaa, 0,leveys/8,rk), Qt::AlignLeft, tr("Yhteensä")  );

    if( alv )
    {
        painter->drawText(QRectF(9 *leveys / 16,0,leveys/8,rk), Qt::AlignRight, QString("%L1 €").arg( ( kokoNetto / 100.0) ,0,'f',2)  );
        painter->drawText(QRectF(12 *leveys / 16,0,leveys/8,rk), Qt::AlignRight, QString("%L1 €").arg( ( kokoVero / 100.0) ,0,'f',2) );
    }
    painter->drawText(QRectF(7*leveys/8,0,leveys/8,rk), Qt::AlignRight, QString("%L1 €").arg( ( model->laskunSumma() / 100.0) ,0,'f',2) );


}

void LaskunTulostaja::erittelyOtsikko(QPagedPaintDevice *printer, QPainter *painter, bool alv)
{
    painter->setFont( QFont("Sans",8));
    qreal rk = painter->fontMetrics().height();
    qreal leveys = painter->window().width();
    double mm = printer->width() * 1.00 / printer->widthMM();

    painter->drawText(QRectF(0,0,leveys/2,rk), Qt::AlignLeft, tr("Nimike"));

    if( alv )
    {
        painter->drawText(QRectF(5 *leveys / 16,0,leveys/8,rk), Qt::AlignHCenter, tr("kpl"));
        painter->drawText(QRectF(7 *leveys / 16,0,leveys/8,rk), Qt::AlignHCenter, tr("á netto"));
        painter->drawText(QRectF(9 *leveys / 16,0,leveys/8,rk), Qt::AlignHCenter, tr("netto"));
        painter->drawText(QRectF(11 *leveys / 16,0,leveys/16,rk), Qt::AlignHCenter, tr("alv"));
        painter->drawText(QRectF(12 *leveys / 16,0,leveys/8,rk), Qt::AlignHCenter, tr("vero"));
    }
    else
    {
        painter->drawText(QRectF(5 *leveys / 8,0,leveys/8,rk), Qt::AlignHCenter, tr("kpl"));
        painter->drawText(QRectF(6 *leveys / 8,0,leveys/8,rk), Qt::AlignHCenter, tr("á hinta"));
    }
    painter->drawText(QRectF(7 *leveys / 8,0,leveys/8,rk), Qt::AlignHCenter, tr("yhteensä"));
    painter->setPen( QPen(QBrush(Qt::black), mm * 0.13));
    painter->drawLine( QLineF( 0, rk, leveys, rk));
    painter->translate(0, rk * 1.1);

}

void LaskunTulostaja::tilisiirto(QPagedPaintDevice *printer, QPainter *painter)
{
    painter->setFont(QFont("Sans", 7));
    double mm = printer->width() * 1.00 / printer->widthMM();

    // QR-koodi
    if( !kp()->asetukset()->onko("LaskuEiQR"))
    {
        QByteArray qrTieto = qrSvg();
        if( !qrTieto.isEmpty())
        {
            QSvgRenderer qrr( qrTieto );
            qrr.render( painter, QRectF( ( printer->widthMM() - 35 ) *mm, 5 * mm, 30 * mm, 30 * mm  ) );
        }
    }

    painter->drawText( QRectF(0,0,mm*19,mm*16.9), Qt::AlignRight | Qt::AlignHCenter, tr("Saajan\n tilinumero\n Mottagarens\n kontonummer"));
    painter->drawText( QRectF(0, mm*18, mm*19, mm*14.8), Qt::AlignRight | Qt::AlignHCenter, tr("Saaja\n Mottagare"));
    painter->drawText( QRectF(0, mm*32.7, mm*19, mm*20), Qt::AlignRight | Qt::AlignTop, tr("Maksajan\n nimi ja\n osoite\n Betalarens\n namn och\n address"));
    painter->drawText( QRectF(0, mm*51.3, mm*19, mm*10), Qt::AlignRight | Qt::AlignBottom , tr("Allekirjoitus\n Underskrift"));
    painter->drawText( QRectF(0, mm*62.3, mm*19, mm*8.5), Qt::AlignRight | Qt::AlignHCenter, tr("Tililtä nro\n Från konto nr"));
    painter->drawText( QRectF(mm * 22, 0, mm*20, mm*10), Qt::AlignLeft, tr("IBAN"));

    painter->drawText( QRectF(mm*112.4, mm*53.8, mm*15, mm*8.5), Qt::AlignLeft | Qt::AlignTop, tr("Viitenumero\nRef.nr."));
    painter->drawText( QRectF(mm*112.4, mm*62.3, mm*15, mm*8.5), Qt::AlignLeft | Qt::AlignTop, tr("Eräpäivä\nFörfallodag"));
    painter->drawText( QRectF(mm*159, mm*62.3, mm*19, mm*8.5), Qt::AlignLeft, tr("Euro"));


    painter->setFont(QFont("Sans",6));
    painter->drawText( QRectF( mm * 140, mm * 72, mm * 60, mm * 20), Qt::AlignLeft | Qt::TextWordWrap, tr("Maksu välitetään saajalle maksujenvälityksen ehtojen "
                                                                                                          "mukaisesti ja vain maksajan ilmoittaman tilinumeron perusteella.\n"
                                                                                                          "Betalning förmedlas till mottagaren enligt villkoren för "
                                                                                                          "betalningsförmedling och endast till det kontonummer som "
                                                                                                          "betalaren angivit.") );
    painter->setPen( QPen( QBrush(Qt::black), mm * 0.5));
    painter->drawLine(QLineF(mm*111.4,0,mm*111.4,mm*69.8));
    painter->drawLine(QLineF(0, mm*16.9, mm*111.4, mm*16.9));
    painter->drawLine(QLineF(0, mm*31.7, mm*111.4, mm*31.7));
    painter->drawLine(QLineF(mm*20, 0, mm*20, mm*31.7));
    painter->drawLine(QLineF(0, mm*61.3, mm*200, mm*61.3));
    painter->drawLine(QLineF(0, mm*69.8, mm*200, mm*69.8));
    painter->drawLine(QLineF(mm*111.4, mm*52.8, mm*200, mm*52.8));
    painter->drawLine(QLineF(mm*131.4, mm*52.8, mm*131.4, mm*69.8));
    painter->drawLine(QLineF(mm*158, mm*61.3, mm*158, mm*69.8));
    painter->drawLine(QLineF(mm*20, mm*61.3, mm*20, mm*69.8));

    painter->setPen( QPen(QBrush(Qt::black), mm * 0.13));
    painter->drawLine( QLineF( mm*22, mm*57.1, mm*108, mm*57.1));

    painter->setPen( QPen(QBrush(Qt::black), mm * 0.13, Qt::DashLine));
    painter->drawLine( QLineF( 0, -1 * mm, painter->window().width(), -1 * mm));

    painter->setFont(QFont("Sans", 10));

    painter->drawText(QRectF( mm*22, mm * 33, mm * 90, mm * 25), Qt::TextWordWrap, model_->osoite());

    painter->drawText( QRectF(mm*133.4, mm*53.8, mm*60, mm*7.5), Qt::AlignLeft | Qt::AlignBottom, model_->viitenumero() );

    painter->drawText( QRectF(mm*133.4, mm*62.3, mm*30, mm*7.5), Qt::AlignLeft | Qt::AlignBottom, model_->erapaiva().toString("dd.MM.yyyy") );
    painter->drawText( QRectF(mm*165, mm*62.3, mm*30, mm*7.5), Qt::AlignRight | Qt::AlignBottom, QString("%L1").arg( (model_->laskunSumma() / 100.0) ,0,'f',2) );

    painter->drawText( QRectF(mm*22, mm*17, mm*90, mm*13), Qt::AlignTop | Qt::TextWordWrap, kp()->asetus("Nimi") + "\n" + kp()->asetus("Osoite")  );
    painter->drawText( QRectF(mm*22, 0, mm*90, mm*17), Qt::AlignVCenter ,  valeilla( iban )  );


    painter->save();
    painter->setFont(QFont("Sans", 7));
    painter->translate(mm * 2, mm* 60);
    painter->rotate(-90.0);
    painter->drawText(0,0,tr("TILISIIRTO. GIRERING"));
    painter->restore();

    // Viivakoodi
    if( !kp()->asetukset()->onko("LaskuEiViivakoodi"))
    {
        painter->save();

        QFont koodifontti( "code128_XL", 36);
        koodifontti.setLetterSpacing(QFont::AbsoluteSpacing, 0.0);
        painter->setFont( koodifontti);
        QString koodi( code128() );
        painter->drawText( QRectF( mm*20, mm*72, mm*100, mm*13), Qt::AlignCenter, koodi  );

        painter->restore();
    }
}

QString LaskunTulostaja::code128() const
{
    QString koodi;
    koodi.append( QChar(210) );   // Code C aloitusmerkki

    int summa = 105;
    int paino = 1;

    QString koodattava = virtuaaliviivakoodi();
    if( koodattava.length() != 54)  // Pitää olla kelpo virtuaalikoodi
        return QString();

    for(int i = 0; i < koodattava.length(); i = i + 2)
    {
        int luku = koodattava.at(i).digitValue()*10 + koodattava.at(i+1).digitValue();
        koodi.append( code128c(luku) );
        summa += paino * luku;
        paino++;
    }

    koodi.append( code128c( summa % 103 ) );
    koodi.append( QChar(211) );

    return koodi;
}

QChar LaskunTulostaja::code128c(int koodattava) const
{
    if( koodattava < 95)
        return QChar( 32 + koodattava);
    else
        return QChar( 105 + koodattava);
}

QByteArray LaskunTulostaja::qrSvg() const
{
    // Esitettävä tieto
    QString data("BCD\n001\n1\nSCT\n");

    QString bic = LaskutModel::bicIbanilla(iban);
    if( bic.isEmpty())
        return QByteArray();
    data.append(bic + "\n");
    data.append(kp()->asetukset()->asetus("Nimi") + "\n");
    data.append(iban + "\n");
    data.append( QString("EUR%1.%2\n\n").arg( model_->laskunSumma() / 100 ).arg( model_->laskunSumma() % 100, 2, 10, QChar('0') ));
    data.append(model_->viitenumero().remove(QChar(' ')) + "\n\n");
    data.append( QString("ReqdExctnDt/%1").arg( model_->erapaiva().toString(Qt::ISODate) ));

    qrcodegen::QrCode qr = qrcodegen::QrCode::encodeText( data.toUtf8().data() , qrcodegen::QrCode::Ecc::QUARTILE);
    return QByteArray::fromStdString( qr.toSvgString(1) );
}

