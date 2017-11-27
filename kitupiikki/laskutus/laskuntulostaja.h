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

#ifndef LASKUNTULOSTAJA_H
#define LASKUNTULOSTAJA_H

#include <QObject>
#include <QPainter>
#include <QPrinter>

#include "laskumodel.h"

class LaskunTulostaja : public QObject
{
    Q_OBJECT
public:
    explicit LaskunTulostaja(LaskuModel *model);

signals:

public slots:
    bool tulosta(QPrinter *printer);


protected:
    void tilisiirto(QPrinter *printer, QPainter *painter);

private:
    LaskuModel *model_;

};

#endif // LASKUNTULOSTAJA_H
