CREATE TABLE asetus (
 avain VARCHAR(40) NOT NULL,
 arvo TEXT) ;

CREATE TABLE tili (
    id     INTEGER      PRIMARY KEY AUTOINCREMENT,
    nro    INTEGER      NOT NULL,
    nimi   VARCHAR (60) NOT NULL,
    tyyppi VARCHAR (10) NOT NULL,
    tila   INTEGER      DEFAULT (1),
    ysiluku INTEGER    NOT NULL,
    json   TEXT
) ;

CREATE INDEX tili_nro ON tili(nro);
CREATE INDEX tili_ysiluku ON tili(ysiluku);

CREATE TABLE tilikausi (
    alkaa  DATE PRIMARY KEY  UNIQUE  NOT NULL,
    loppuu DATE UNIQUE  NOT NULL,
    json   TEXT
);

CREATE TABLE tositelaji (
    id       INTEGER         PRIMARY KEY AUTOINCREMENT,
    tunnus   VARCHAR(5)      UNIQUE NOT NULL,
    nimi     VARCHAR (60)    NOT NULL,
    json     TEXT
);

INSERT INTO tositelaji ( id, tunnus,nimi) VALUES
  (0,'***','Järjestelmän tosite'),
  (1,'','Muu tosite' );

CREATE TABLE tosite (
    id        INTEGER      PRIMARY KEY AUTOINCREMENT,
    pvm       DATE         NOT NULL,
    otsikko   TEXT,
    kommentti TEXT,
    tunniste  INTEGER,
    tiliote   INTEGER      REFERENCES tili (id) ON UPDATE CASCADE,
    lasku     INTEGER      REFERENCES lasku (id) ON UPDATE CASCADE,
    laji      INTEGER         REFERENCES tositelaji (id)
                                 DEFAULT (1),
    json      TEXT
);

CREATE INDEX tosite_pvm_index ON tosite(pvm);
CREATE INDEX tosite_tiliote_index on tosite(tiliote);
CREATE INDEX tosite_laji_index on tosite(laji);
CREATE INDEX tosite_tunniste_index on tosite(tunniste);


CREATE TABLE kohdennus (
    id     INTEGER      PRIMARY KEY AUTOINCREMENT,
    nimi   VARCHAR (60) NOT NULL,
    alkaa  DATE,
    loppuu DATE,
    tyyppi INTEGER  NOT NULL
);

INSERT INTO kohdennus(id, nimi, tyyppi) VALUES(0,"(Ei kohdennusta)",0);


CREATE TABLE vienti (
    id              INTEGER PRIMARY KEY AUTOINCREMENT,
    tosite          INTEGER NOT NULL
                            REFERENCES tosite (id),
    vientirivi      INTEGER NOT NULL,
    pvm             DATE    NOT NULL,
    tili            INTEGER REFERENCES tili (id) ON DELETE RESTRICT
                                                 ON UPDATE RESTRICT,
    debetsnt        BIGINT,
    kreditsnt       BIGINT,
    selite          TEXT,
    alvkoodi        INTEGER DEFAULT(0),
    alvprosentti    INTEGER DEFAULT(0),
    kohdennus        INTEGER REFERENCES kohdennus (id) ON DELETE RESTRICT
                                                      ON UPDATE CASCADE,
    eraid           INTEGER REFERENCES vienti(id) ON DELETE RESTRICT
                                                  ON UPDATE CASCADE,
    json            TEXT,
    luotu           DATETIME,
    muokattu        DATETIME
);

CREATE INDEX vienti_tosite_index ON vienti(tosite);
CREATE INDEX vienti_pvm_index ON vienti(pvm);
CREATE INDEX vienti_tili_index ON vienti(tili);
CREATE INDEX vienti_kodennus_index ON vienti(kohdennus);
CREATE INDEX vienti_taseera_index ON vienti(eraid);

CREATE TABLE liite (
    id       INTEGER      PRIMARY KEY AUTOINCREMENT,
    liiteno  INTEGER      NOT NULL,
    tosite   INTEGER      REFERENCES tosite (id) ON DELETE RESTRICT
                                                 ON UPDATE RESTRICT,
    otsikko  TEXT,
    sha      TEXT         NOT NULL,
    peukku   BLOB
);

CREATE TABLE lasku (
    id          INTEGER        PRIMARY KEY AUTOINCREMENT,
    viite       VARCHAR(20),
    laskupvm    DATE,
    erapvm      DATE,
    summaSnt    BIGINT,
    avoinSnt    BIGINT,
    asiakas     TEXT,
    json        TEXT

);

CREATE INDEX lasku_viite ON lasku(viite);
CREATE INDEX lasku_pvm ON lasku(laskupvm);
CREATE INDEX lasku_erapvm ON lasku(erapvm);
CREATE INDEX lasku_asiakas ON lasku(asiakas);


CREATE VIEW vientivw AS
    SELECT vienti.id as vientiId,
           vienti.pvm as pvm,
           vienti.debetsnt as debetsnt,
           vienti.kreditsnt as kreditsnt,
           vienti.selite as selite,
           vienti.kohdennus as kohdennusId,
           kohdennus.nimi as kohdennus,
           tositelaji.tunnus as tositelaji,
           tosite.tunniste as tunniste,
           tosite.id as tositeId,
           tositelaji.id as tositelajiId,
           tili.nro as tilinro,
           tili.nimi as tilinimi,
           tili.tyyppi as tilityyppi
      FROM vienti,
           tosite,
           tili,
           tositelaji,
           kohdennus
     WHERE vienti.tosite = tosite.id AND
           vienti.tili = tili.id AND
           tosite.laji = tositelaji.id AND
           vienti.kohdennus = kohdennus.id ;
