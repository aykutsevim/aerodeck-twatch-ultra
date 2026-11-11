// core::Metar: METAR decoder, ceiling, flight category, observation time, staleness, formatting (S3 data-model §1,
// research R5/R6). The corpus below is real: collected from aviationweather.gov on 2026-11-11 (format=json, hours=24)
// with the provider's decoded fields (fltCat, wdir, wspd, wgst, visib, altim, temp, dewp) used as the oracle.
#include <unity.h>
#include <cstring>
#include <string>
#include "core/ClockMath.h"
#include "core/Metar.h"

using namespace core;
using Metar::Category;

void setUp() {}
void tearDown() {}

constexpr int N = -1;                         // missing
constexpr int VRB = Metar::kWindVrb;
constexpr int NT = Metar::kNoTemp;
constexpr int32_t NC = Metar::kNoCeiling;
constexpr int32_t CU = Metar::kCeilingUnknown;

struct Case {
    const char* raw;
    const char* stn;
    int day, hour, minute;
    int dir, spd, gust, varFrom, varTo;
    int32_t visM;
    const char* visText;
    int32_t ceil;
    int t, d, qnh;
    Category cat;     // == provider fltCat
};

// clang-format off
static const Case kCorpus[] = {
    {"METAR LTAC 111250Z 27007KT 9999 FEW016 BKN030 10/04 Q1020 NOSIG",
     "LTAC", 11, 12, 50, 270, 7, N, N, N, 10000, "10 km+", 3000, 10, 4, 1020, Category::Mvfr},
    {"METAR LTFM 111250Z 22021KT 9999 SCT032 BKN100 18/12 Q1014 NOSIG RMK RWY17L 22020G30KT RWY34L 21020KT RWY16R 22018KT RWY36 22017KT RWY18 22018KT",
     "LTFM", 11, 12, 50, 220, 21, N, N, N, 10000, "10 km+", 10000, 18, 12, 1014, Category::Vfr},
    {"METAR ENGM 111250Z VRB02KT 9999 VCSH SCT003 BKN005 M01/M01 Q0998 TEMPO BKN004",
     "ENGM", 11, 12, 50, VRB, 2, N, N, N, 10000, "10 km+", 500, -1, -1, 998, Category::Ifr},
    {"METAR LEMD 111300Z 14012KT 100V160 8000 -RA FEW014 BKN035 OVC055 14/13 Q1004 NOSIG",
     "LEMD", 11, 13, 0, 140, 12, N, 100, 160, 8000, "8 km", 3500, 14, 13, 1004, Category::Mvfr},
    {"METAR KSFO 111256Z 00000KT 1/4SM R28R/1400VP6000FT FG VV002 13/13 A3008 RMK AO2 SLP185 T01280128 $",
     "KSFO", 11, 12, 56, 0, 0, N, N, N, 402, "1/4 SM", 200, 13, 13, 1019, Category::Lifr},
    {"METAR UNNT 111300Z 23008MPS 9999 OVC026 01/M04 Q1020 R25/CLRD// R16/CLRD// NOSIG RMK QFE755/1007",
     "UNNT", 11, 13, 0, 230, 16, N, N, N, 10000, "10 km+", 2600, 1, -4, 1020, Category::Mvfr},
    {"METAR BIKF 111300Z 04015KT CAVOK M00/M09 Q1022",
     "BIKF", 11, 13, 0, 40, 15, N, N, N, 10000, "CAVOK", NC, 0, -9, 1022, Category::Vfr},
    {"SPECI KSFO 111313Z 20003KT 2 1/2SM BR BKN002 13/13 A3007 RMK AO2 T01280128 $",
     "KSFO", 11, 13, 13, 200, 3, N, N, N, 4023, "2 1/2 SM", 200, 13, 13, 1018, Category::Lifr},
    {"SPECI KSFO 111237Z 00000KT M1/4SM R28R/1000V1400FT FG VV002 13/13 A3008 RMK AO2 T01280128 $",
     "KSFO", 11, 12, 37, 0, 0, N, N, N, 402, "<1/4 SM", 200, 13, 13, 1019, Category::Lifr},
    {"METAR CYZF 111300Z 12015KT 1 1/4SM -SN DRSN BKN006 OVC010 M02/M02 A2977 RMK ST7SC1 SLP101",
     "CYZF", 11, 13, 0, 120, 15, N, N, N, 2012, "1 1/4 SM", 600, -2, -2, 1008, Category::Ifr},
    {"METAR UUEE 111300Z 18004MPS 150V210 9999 SCT025 03/M02 Q1011 R24L/220151 R24C/220151 NOSIG",
     "UUEE", 11, 13, 0, 180, 8, N, 150, 210, 10000, "10 km+", NC, 3, -2, 1011, Category::Vfr},
    {"METAR CYYZ 111300Z 30007KT 15SM FEW170 FEW250 06/04 A2995 RMK AC1CI2 AC TR SLP147",
     "CYYZ", 11, 13, 0, 300, 7, N, N, N, 24140, "15 SM", NC, 6, 4, 1014, Category::Vfr},
    {"METAR YSSY 111300Z AUTO 17013KT 9999 // NCD 20/18 Q1019",
     "YSSY", 11, 13, 0, 170, 13, N, N, N, 10000, "10 km+", NC, 20, 18, 1019, Category::Vfr},
    {"METAR ZBAA 111300Z VRB01MPS 6000 NSC 03/01 Q1021 NOSIG",
     "ZBAA", 11, 13, 0, VRB, 2, N, N, N, 6000, "6 km", NC, 3, 1, 1021, Category::Mvfr},
    {"METAR CYYZ 111000Z 29005KT 15SM SKC 04/03 A2993 RMK SLP142",
     "CYYZ", 11, 10, 0, 290, 5, N, N, N, 24140, "15 SM", NC, 4, 3, 1014, Category::Vfr},
    {"METAR EGLL 110850Z AUTO 06007KT 9999 -RA BKN033/// OVC041/// //////TCU 09/07 Q1004 NOSIG",
     "EGLL", 11, 8, 50, 60, 7, N, N, N, 10000, "10 km+", 3300, 9, 7, 1004, Category::Vfr},
    {"METAR UUEE 110530Z 20005MPS 9000 3000SW -SHSN SCT007 BKN039CB 01/M00 Q1014 R24L/290145 R24C/290145 NOSIG",
     "UUEE", 11, 5, 30, 200, 10, N, N, N, 9000, "9 km", 3900, 1, 0, 1014, Category::Vfr},
    {"METAR EDDF 111250Z AUTO 24003KT 170V280 3300 -RADZ BR OVC003 07/07 Q1002 NOSIG",
     "EDDF", 11, 12, 50, 240, 3, N, 170, 280, 3300, "3300 m", 300, 7, 7, 1002, Category::Lifr},
    {"METAR EHAM 111255Z 06007KT 010V100 8000 FEW005 BKN007 05/04 Q1003 NOSIG",
     "EHAM", 11, 12, 55, 60, 7, N, 10, 100, 8000, "8 km", 700, 5, 4, 1003, Category::Ifr},
    {"METAR LFPG 111300Z 22009KT 9999 BKN013 BKN017 14/12 Q1002 NOSIG",
     "LFPG", 11, 13, 0, 220, 9, N, N, N, 10000, "10 km+", 1300, 14, 12, 1002, Category::Mvfr},
    {"SPECI CYZF 111221Z 12014G19KT 1 1/4SM -SN DRSN BKN006 OVC010 M03/M03 A2979 RMK ST6ST2 SLP107",
     "CYZF", 11, 12, 21, 120, 14, 19, N, N, 2012, "1 1/4 SM", 600, -3, -3, 1009, Category::Ifr},
    {"METAR EGLL 110750Z AUTO 05006KT 020V080 7000 -RA OVC035/// //////TCU 09/08 Q1003 TEMPO BKN009",
     "EGLL", 11, 7, 50, 50, 6, N, 20, 80, 7000, "7 km", 3500, 9, 8, 1003, Category::Mvfr},
    {"METAR EGLL 111250Z COR AUTO 05007KT 9999 SCT014 OVC045 10/08 Q1004 NOSIG",
     "EGLL", 11, 12, 50, 50, 7, N, N, N, 10000, "10 km+", 4500, 10, 8, 1004, Category::Vfr},
    {"METAR ENGM 111220Z VRB02KT 9999 -SN BKN004 M00/M01 Q0998 TEMPO SCT004 BKN020",
     "ENGM", 11, 12, 20, VRB, 2, N, N, N, 10000, "10 km+", 400, 0, -1, 998, Category::Lifr},
};
// clang-format on

static Metar::Report D(const std::string& s)
{
    Metar::Report r;
    Metar::decode(s.c_str(), r);
    return r;
}

// Synthetic report around one group (header + defaults that never change the tested value).
static Metar::Report G(const char* groups)
{
    return D(std::string("METAR KXYZ 111250Z ") + groups);
}

static void test_corpus()
{
    char msg[160];
    for (const Case& c : kCorpus) {
        Metar::Report r;
        snprintf(msg, sizeof msg, "%s", c.raw);
        TEST_ASSERT_TRUE_MESSAGE(Metar::decode(c.raw, r), msg);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c.stn, r.station, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.day, r.day, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.hour, r.hour, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.minute, r.minute, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.dir, r.windDir, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.spd, r.windSpdKt, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.gust, r.windGustKt, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.varFrom, r.windVarFrom, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.varTo, r.windVarTo, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.visM, r.visM, msg);
        TEST_ASSERT_EQUAL_STRING_MESSAGE(c.visText, r.visText, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.ceil, Metar::ceilingFt(r), msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.t, r.tempC, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.d, r.dewC, msg);
        TEST_ASSERT_EQUAL_MESSAGE(c.qnh, r.qnhHpa, msg);
        TEST_ASSERT_EQUAL_MESSAGE(int(c.cat), int(Metar::category(r)), msg);
        TEST_ASSERT_FALSE_MESSAGE(r.nil, msg);
    }
    TEST_ASSERT_TRUE(sizeof(kCorpus) / sizeof(kCorpus[0]) >= 20);   // FR-018
}

static void test_corpus_details()
{
    Metar::Report r = D(kCorpus[0].raw);
    TEST_ASSERT_FALSE(r.qnhWasInHg);
    TEST_ASSERT_EQUAL(2, r.nClouds);
    TEST_ASSERT_EQUAL(int(Metar::Cover::Few), int(r.clouds[0].cover));
    TEST_ASSERT_EQUAL(1600, r.clouds[0].baseFt);
    r = D(kCorpus[4].raw);   // KSFO A3008, VV002
    TEST_ASSERT_TRUE(r.qnhWasInHg);
    TEST_ASSERT_TRUE(r.visIsSm);
    TEST_ASSERT_EQUAL(int(Metar::Cover::Vv), int(r.clouds[0].cover));
    r = D(kCorpus[6].raw);   // BIKF CAVOK
    TEST_ASSERT_TRUE(r.cavok);
    r = D("METAR UUEE 110530Z 20005MPS 9000 3000SW -SHSN SCT007 BKN039CB 01/M00 Q1014");
    TEST_ASSERT_TRUE(r.clouds[1].cb);
    r = D("SPECI CYZF 111221Z 12014G19KT 1 1/4SM -SN BKN006 M03/M03 A2979");
    TEST_ASSERT_TRUE(r.visIsSm);
}

static void test_wind()
{
    Metar::Report r = G("27015G25KT 9999 SKC 10/05 Q1013");
    TEST_ASSERT_EQUAL(270, r.windDir);
    TEST_ASSERT_EQUAL(15, r.windSpdKt);
    TEST_ASSERT_EQUAL(25, r.windGustKt);
    r = G("VRB03KT 9999 SKC 10/05 Q1013");
    TEST_ASSERT_EQUAL(VRB, r.windDir);
    TEST_ASSERT_EQUAL(3, r.windSpdKt);
    r = G("27006KT 250V310 9999 SKC 10/05 Q1013");
    TEST_ASSERT_EQUAL(250, r.windVarFrom);
    TEST_ASSERT_EQUAL(310, r.windVarTo);
    r = G("00000KT 9999 SKC 10/05 Q1013");
    TEST_ASSERT_EQUAL(0, r.windDir);
    TEST_ASSERT_EQUAL(0, r.windSpdKt);
    r = G("05008MPS 9999 SKC 10/05 Q1013");
    TEST_ASSERT_EQUAL(50, r.windDir);
    TEST_ASSERT_EQUAL(16, r.windSpdKt);
    r = G("/////KT 9999 SKC 10/05 Q1013");
    TEST_ASSERT_EQUAL(N, r.windDir);
    TEST_ASSERT_EQUAL(N, r.windSpdKt);
    TEST_ASSERT_EQUAL(10000, r.visM);   // decoding continues after a missing wind
    r = G("36099KT 9999 SKC 10/05 Q1013");
    TEST_ASSERT_EQUAL(360, r.windDir);
    TEST_ASSERT_EQUAL(99, r.windSpdKt);
    r = G("36300KT 9999 SKC 10/05 Q1013");   // direction 363: out of range → missing
    TEST_ASSERT_EQUAL(N, r.windDir);
    TEST_ASSERT_EQUAL(N, r.windSpdKt);
    r = G("270120G150KT 9999 SKC 10/05 Q1013");   // three-digit speed and gust
    TEST_ASSERT_EQUAL(120, r.windSpdKt);
    TEST_ASSERT_EQUAL(150, r.windGustKt);
}

static void test_visibility()
{
    Metar::Report r = G("27005KT 0800 FG VV001 10/10 Q1013");
    TEST_ASSERT_EQUAL(800, r.visM);
    TEST_ASSERT_EQUAL_STRING("800 m", r.visText);
    r = G("27005KT 1 1/2SM BR OVC004 10/10 A2992");
    TEST_ASSERT_EQUAL(2414, r.visM);
    TEST_ASSERT_EQUAL_STRING("1 1/2 SM", r.visText);
    r = G("27005KT P6SM SKC 10/05 A2992");
    TEST_ASSERT_EQUAL(9656, r.visM);
    TEST_ASSERT_EQUAL_STRING("6+ SM", r.visText);
    r = G("27005KT 10SM SKC 10/05 A2992");
    TEST_ASSERT_EQUAL(16093, r.visM);
    TEST_ASSERT_EQUAL_STRING("10 SM", r.visText);
    r = G("27005KT 3/4SM BR OVC004 10/10 A2992");
    TEST_ASSERT_EQUAL(1207, r.visM);
    TEST_ASSERT_EQUAL_STRING("3/4 SM", r.visText);
    r = G("27005KT //// BKN030 10/05 Q1013");
    TEST_ASSERT_EQUAL(N, r.visM);
    TEST_ASSERT_EQUAL_STRING("", r.visText);
    TEST_ASSERT_EQUAL(3000, Metar::ceilingFt(r));   // decoding continues
    r = G("27005KT 4000 1500NE BR BKN030 10/05 Q1013");
    TEST_ASSERT_EQUAL(4000, r.visM);
    TEST_ASSERT_EQUAL_STRING("4000 m", r.visText);
    r = G("27005KT 5500 BKN030 10/05 Q1013");
    TEST_ASSERT_EQUAL_STRING("5.5 km", r.visText);
}

static void test_clouds_and_ceiling()
{
    TEST_ASSERT_EQUAL(800, Metar::ceilingFt(G("27005KT 9999 SCT040 OVC008 10/05 Q1013")));
    TEST_ASSERT_EQUAL(200, Metar::ceilingFt(G("27005KT 0200 FG VV002 10/10 Q1013")));
    TEST_ASSERT_EQUAL(NC, Metar::ceilingFt(G("27005KT 9999 NSC 10/05 Q1013")));
    TEST_ASSERT_EQUAL(NC, Metar::ceilingFt(G("27005KT 10SM SKC 10/05 A2992")));
    TEST_ASSERT_EQUAL(NC, Metar::ceilingFt(G("27005KT 10SM CLR 10/05 A2992")));
    TEST_ASSERT_EQUAL(NC, Metar::ceilingFt(G("27005KT 9999 NCD 10/05 Q1013")));
    TEST_ASSERT_EQUAL(NC, Metar::ceilingFt(G("27005KT CAVOK 10/05 Q1013")));
    TEST_ASSERT_EQUAL(NC, Metar::ceilingFt(G("27005KT 10SM FEW020 SCT040 10/05 A2992")));
    Metar::Report r = G("27005KT 9999 BKN020CB 10/05 Q1013");
    TEST_ASSERT_EQUAL(2000, Metar::ceilingFt(r));
    TEST_ASSERT_TRUE(r.clouds[0].cb);
    r = G("27005KT 9999 ///015 10/05 Q1013");   // unknown cover: not a ceiling, not cloud info
    TEST_ASSERT_EQUAL(1, r.nClouds);
    TEST_ASSERT_EQUAL(int(Metar::Cover::Unknown), int(r.clouds[0].cover));
    TEST_ASSERT_EQUAL(1500, r.clouds[0].baseFt);
    TEST_ASSERT_EQUAL(CU, Metar::ceilingFt(r));
    TEST_ASSERT_EQUAL(CU, Metar::ceilingFt(G("27005KT 9999 BKN/// 10/05 Q1013")));   // ceiling height unknown
    TEST_ASSERT_EQUAL(CU, Metar::ceilingFt(G("27005KT 9999 10/05 Q1013")));          // no cloud group at all
}

static void test_temperature_pressure_kind()
{
    Metar::Report r = G("27005KT 9999 SKC M05/M10 Q1013");
    TEST_ASSERT_EQUAL(-5, r.tempC);
    TEST_ASSERT_EQUAL(-10, r.dewC);
    r = G("27005KT 9999 SKC 10/M01 Q1013");
    TEST_ASSERT_EQUAL(10, r.tempC);
    TEST_ASSERT_EQUAL(-1, r.dewC);
    r = G("27005KT 9999 SKC ///// Q1013");
    TEST_ASSERT_EQUAL(NT, r.tempC);
    TEST_ASSERT_EQUAL(NT, r.dewC);
    r = G("27005KT 9999 SKC 10/// Q1013");
    TEST_ASSERT_EQUAL(10, r.tempC);
    TEST_ASSERT_EQUAL(NT, r.dewC);
    TEST_ASSERT_EQUAL(1013, G("27005KT 9999 SKC 10/05 A2992").qnhHpa);
    TEST_ASSERT_EQUAL(1020, G("27005KT 9999 SKC 10/05 A3012").qnhHpa);
    TEST_ASSERT_EQUAL(999, G("27005KT 9999 SKC 10/05 Q0999").qnhHpa);
    TEST_ASSERT_EQUAL(N, G("27005KT 9999 SKC 10/05 Q////").qnhHpa);
    TEST_ASSERT_EQUAL(N, G("27005KT 9999 SKC 10/05 Q0500").qnhHpa);   // out of range
    r = D("METAR LTAC 111020Z NIL");
    TEST_ASSERT_TRUE(r.nil);
    TEST_ASSERT_EQUAL_STRING("LTAC", r.station);
    r = D("SPECI KXYZ 111252Z COR AUTO 27005KT 9999 SKC 10/05 Q1013");
    TEST_ASSERT_EQUAL_STRING("KXYZ", r.station);
    TEST_ASSERT_EQUAL(270, r.windDir);
    r = D("KXYZ 111252Z 27005KT 9999 SKC 10/05 Q1013");   // no METAR/SPECI prefix
    TEST_ASSERT_EQUAL_STRING("KXYZ", r.station);
    // Groups after RMK / NOSIG / TEMPO / BECMG are not decoded.
    r = G("27005KT 9999 SKC 10/05 Q1013 TEMPO 4000 BKN008");
    TEST_ASSERT_EQUAL(10000, r.visM);
    TEST_ASSERT_EQUAL(NC, Metar::ceilingFt(r));
    r = G("27005KT 9999 SKC 10/05 Q1013 BECMG 25020G35KT");
    TEST_ASSERT_EQUAL(5, r.windSpdKt);
    TEST_ASSERT_EQUAL(N, r.windGustKt);
    r = G("27005KT 9999 FEW030 10/05 Q1013 RMK BKN005");
    TEST_ASSERT_EQUAL(NC, Metar::ceilingFt(r));
}

static Category cat(const char* groups) { return Metar::category(G(groups)); }

static void test_category_boundaries()
{
    // Ceiling (10SM visibility)
    TEST_ASSERT_EQUAL(int(Category::Mvfr), int(cat("27005KT 10SM BKN030 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Vfr), int(cat("27005KT 10SM BKN031 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Mvfr), int(cat("27005KT 10SM BKN010 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Ifr), int(cat("27005KT 10SM BKN009 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Ifr), int(cat("27005KT 10SM OVC005 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Lifr), int(cat("27005KT 10SM OVC004 10/05 A2992")));
    // Visibility in statute miles (sky clear)
    TEST_ASSERT_EQUAL(int(Category::Mvfr), int(cat("27005KT 5SM SKC 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Vfr), int(cat("27005KT 6SM SKC 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Mvfr), int(cat("27005KT 3SM SKC 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Ifr), int(cat("27005KT 2SM SKC 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Ifr), int(cat("27005KT 1SM SKC 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Lifr), int(cat("27005KT 3/4SM SKC 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Lifr), int(cat("27005KT M1/4SM SKC 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Vfr), int(cat("27005KT P6SM SKC 10/05 A2992")));
    // Visibility in metres
    TEST_ASSERT_EQUAL(int(Category::Mvfr), int(cat("27005KT 8000 NSC 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Vfr), int(cat("27005KT 9000 NSC 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Mvfr), int(cat("27005KT 4800 NSC 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Ifr), int(cat("27005KT 4700 NSC 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Ifr), int(cat("27005KT 1600 NSC 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Lifr), int(cat("27005KT 1500 NSC 10/05 Q1013")));
    // The worse of the two wins
    TEST_ASSERT_EQUAL(int(Category::Lifr), int(cat("27005KT 9999 BKN004 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Lifr), int(cat("27005KT 1500 SKC 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Ifr), int(cat("27005KT 3000 BKN020 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Vfr), int(cat("27005KT CAVOK 10/05 Q1013")));
    // Unknown rule (spec FR-007)
    TEST_ASSERT_EQUAL(int(Category::Unknown), int(cat("27005KT //// BKN030 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Unknown), int(cat("27005KT 10SM 10/05 A2992")));     // no cloud info
    TEST_ASSERT_EQUAL(int(Category::Lifr), int(cat("27005KT 1/2SM 10/05 A2992")));       // vis alone is LIFR
    TEST_ASSERT_EQUAL(int(Category::Unknown), int(cat("27005KT 2SM 10/05 A2992")));      // IFR, could be LIFR
    TEST_ASSERT_EQUAL(int(Category::Unknown), int(cat("27005KT 9999 ///015 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Unknown), int(cat("27005KT 9999 BKN/// 10/05 Q1013")));
    TEST_ASSERT_EQUAL(int(Category::Vfr), int(cat("27005KT 10SM FEW020 SCT040 10/05 A2992")));
    TEST_ASSERT_EQUAL(int(Category::Unknown), int(Metar::category(D("METAR LTAC 111020Z NIL"))));
    TEST_ASSERT_EQUAL_STRING("MVFR", Metar::categoryName(Category::Mvfr));
    TEST_ASSERT_EQUAL_STRING("--", Metar::categoryName(Category::Unknown));
}

static void test_resolve_obs_time()
{
    const int64_t now = utcFromCivil(2026, 11, 11, 13, 5, 0);
    TEST_ASSERT_EQUAL_INT64(utcFromCivil(2026, 11, 11, 12, 50, 0), Metar::resolveObsTime(11, 12, 50, now));
    TEST_ASSERT_EQUAL_INT64(utcFromCivil(2026, 11, 10, 23, 50, 0), Metar::resolveObsTime(10, 23, 50, now));
    // Month end: day 31 seen on Nov 1 → Oct 31; day 30 seen on Dec 1 → Nov 30.
    TEST_ASSERT_EQUAL_INT64(utcFromCivil(2026, 10, 31, 23, 50, 0),
                            Metar::resolveObsTime(31, 23, 50, utcFromCivil(2026, 11, 1, 0, 5, 0)));
    TEST_ASSERT_EQUAL_INT64(utcFromCivil(2026, 11, 30, 23, 50, 0),
                            Metar::resolveObsTime(30, 23, 50, utcFromCivil(2026, 12, 1, 0, 10, 0)));
    // Day 31 seen on Dec 1: November has no 31st → October 31.
    TEST_ASSERT_EQUAL_INT64(utcFromCivil(2026, 10, 31, 23, 50, 0),
                            Metar::resolveObsTime(31, 23, 50, utcFromCivil(2026, 12, 1, 0, 10, 0)));
    // Year rollover.
    TEST_ASSERT_EQUAL_INT64(utcFromCivil(2026, 12, 31, 23, 50, 0),
                            Metar::resolveObsTime(31, 23, 50, utcFromCivil(2027, 1, 1, 0, 5, 0)));
    // Up to 10 min in the future is allowed (slow watch clock); 20 min means last month.
    TEST_ASSERT_EQUAL_INT64(utcFromCivil(2026, 11, 11, 13, 10, 0), Metar::resolveObsTime(11, 13, 10, now));
    TEST_ASSERT_EQUAL_INT64(utcFromCivil(2026, 10, 11, 13, 25, 0), Metar::resolveObsTime(11, 13, 25, now));
    TEST_ASSERT_EQUAL_INT64(0, Metar::resolveObsTime(11, 12, 50, 0));
    TEST_ASSERT_EQUAL_INT64(0, Metar::resolveObsTime(-1, 12, 50, now));
    TEST_ASSERT_EQUAL_INT64(0, Metar::resolveObsTime(32, 12, 50, now));
}

static void test_is_stale()
{
    const int64_t obs = utcFromCivil(2026, 11, 11, 12, 0, 0);
    TEST_ASSERT_FALSE(Metar::isStale(obs, obs + 89 * 60 + 59));
    TEST_ASSERT_FALSE(Metar::isStale(obs, obs + 90 * 60));
    TEST_ASSERT_TRUE(Metar::isStale(obs, obs + 90 * 60 + 1));
    TEST_ASSERT_TRUE(Metar::isStale(0, obs));
    TEST_ASSERT_TRUE(Metar::isStale(obs, 0));
}

static void test_format_helpers()
{
    char b[40];
    Metar::formatWind(G("27015G25KT 9999 SKC 10/05 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("270\xC2\xB0 15G25 kt", b);
    Metar::formatWind(G("VRB03KT 9999 SKC 10/05 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("VRB 03 kt", b);
    Metar::formatWind(G("27006KT 250V310 9999 SKC 10/05 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("VRB 250-310\xC2\xB0 06 kt", b);
    Metar::formatWind(G("00000KT 9999 SKC 10/05 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("CALM", b);
    Metar::formatWind(G("/////KT 9999 SKC 10/05 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("--", b);
    Metar::formatWind(G("05008KT 9999 SKC 10/05 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("050\xC2\xB0 08 kt", b);

    Metar::formatClouds(D(kCorpus[0].raw), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("FEW016 BKN030", b);
    Metar::formatClouds(D(kCorpus[4].raw), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("VV002", b);
    Metar::formatClouds(D(kCorpus[6].raw), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("CAVOK", b);
    Metar::formatClouds(D(kCorpus[13].raw), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("NSC", b);
    Metar::formatClouds(D(kCorpus[15].raw), b, sizeof b);   // EGLL: unknown-cover layer skipped
    TEST_ASSERT_EQUAL_STRING("BKN033 OVC041", b);
    Metar::formatClouds(D("METAR UUEE 110530Z 20005MPS 9000 SCT007 BKN039CB 01/M00 Q1014"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("SCT007 BKN039CB", b);
    Metar::formatClouds(G("27005KT 9999 FEW010 SCT020 BKN030 OVC040 10/05 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("FEW010 SCT020 BKN030", b);   // at most 3 layers
    Metar::formatClouds(G("27005KT 9999 10/05 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("--", b);
    char tiny[6];
    Metar::formatClouds(D(kCorpus[0].raw), tiny, sizeof tiny);   // truncated, NUL-terminated
    TEST_ASSERT_EQUAL(5, int(strlen(tiny)));
}

static void test_readable_text()
{
    char b[96];
    Metar::formatCloudsText(D("METAR LTAC 111350Z 33004KT 9999 SCT035 BKN180 09/04 Q1019 NOSIG"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("Scattered 3,500 ft, broken 18,000 ft", b);
    Metar::formatCloudsText(D(kCorpus[0].raw), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("Few 1,600 ft, broken 3,000 ft", b);
    Metar::formatCloudsText(D(kCorpus[4].raw), b, sizeof b);   // KSFO VV002
    TEST_ASSERT_EQUAL_STRING("Sky obscured, vertical visibility 200 ft", b);
    Metar::formatCloudsText(D(kCorpus[6].raw), b, sizeof b);   // BIKF CAVOK
    TEST_ASSERT_EQUAL_STRING("CAVOK: no cloud below 5,000 ft", b);
    Metar::formatCloudsText(D(kCorpus[13].raw), b, sizeof b);  // ZBAA NSC
    TEST_ASSERT_EQUAL_STRING("No significant cloud", b);
    Metar::formatCloudsText(D(kCorpus[12].raw), b, sizeof b);  // YSSY NCD
    TEST_ASSERT_EQUAL_STRING("No cloud detected", b);
    Metar::formatCloudsText(D(kCorpus[14].raw), b, sizeof b);  // CYYZ SKC
    TEST_ASSERT_EQUAL_STRING("Sky clear", b);
    Metar::formatCloudsText(D("METAR UUEE 110530Z 20005MPS 9000 SCT007 BKN039CB 01/M00 Q1014"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("Scattered 700 ft, broken 3,900 ft (CB)", b);
    Metar::formatCloudsText(G("27005KT 9999 BKN/// 10/05 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("Broken height ?", b);
    Metar::formatCloudsText(G("27005KT 9999 10/05 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("--", b);
    char tiny[8];
    Metar::formatCloudsText(D(kCorpus[0].raw), tiny, sizeof tiny);
    TEST_ASSERT_EQUAL(7, int(strlen(tiny)));

    Metar::formatTemp(D(kCorpus[0].raw), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("10\xC2\xB0" "C, dew 4\xC2\xB0" "C", b);
    Metar::formatTemp(G("27005KT 9999 SKC M05/M10 Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("-5\xC2\xB0" "C, dew -10\xC2\xB0" "C", b);
    Metar::formatTemp(G("27005KT 9999 SKC 10/// Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("10\xC2\xB0" "C", b);
    Metar::formatTemp(G("27005KT 9999 SKC ///// Q1013"), b, sizeof b);
    TEST_ASSERT_EQUAL_STRING("--", b);
}

static void test_pick_newest()
{
    char out[300];
    const std::string body =
        "METAR LTAC 111250Z 27007KT 9999 FEW016 BKN030 10/04 Q1020 NOSIG\r\n"
        "METAR LTAC 111220Z 25005KT 210V280 9999 FEW016 BKN030 10/04 Q1020 NOSIG\r\n";
    TEST_ASSERT_TRUE(Metar::pickNewest(body.c_str(), body.size(), "LTAC", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("METAR LTAC 111250Z 27007KT 9999 FEW016 BKN030 10/04 Q1020 NOSIG", out);
    TEST_ASSERT_TRUE(Metar::pickNewest(body.c_str(), body.size(), "ltac", out, sizeof out));   // case-insensitive
    TEST_ASSERT_FALSE(Metar::pickNewest(body.c_str(), body.size(), "LTFM", out, sizeof out));
    const std::string other = "\nMETAR LTFM 111250Z 22021KT 9999 SCT032 18/12 Q1014\nMETAR LTAC 111250Z NIL\n";
    TEST_ASSERT_TRUE(Metar::pickNewest(other.c_str(), other.size(), "LTAC", out, sizeof out));
    TEST_ASSERT_EQUAL_STRING("METAR LTAC 111250Z NIL", out);
    const std::string longLine = "METAR LTAC 111250Z 27007KT " + std::string(400, 'X');
    TEST_ASSERT_TRUE(Metar::pickNewest(longLine.c_str(), longLine.size(), "LTAC", out, sizeof out));
    TEST_ASSERT_EQUAL(255, int(strlen(out)));
    char small[16];
    TEST_ASSERT_TRUE(Metar::pickNewest(body.c_str(), body.size(), "LTAC", small, sizeof small));
    TEST_ASSERT_EQUAL(15, int(strlen(small)));
    TEST_ASSERT_FALSE(Metar::pickNewest("", 0, "LTAC", out, sizeof out));
    TEST_ASSERT_FALSE(Metar::pickNewest(nullptr, 0, "LTAC", out, sizeof out));
}

static void test_robustness()
{
    Metar::Report r;
    TEST_ASSERT_FALSE(Metar::decode("", r));
    TEST_ASSERT_FALSE(Metar::decode("     ", r));
    TEST_ASSERT_FALSE(Metar::decode(nullptr, r));
    TEST_ASSERT_EQUAL(N, r.visM);
    std::string a(255, 'A');
    TEST_ASSERT_FALSE(Metar::decode(a.c_str(), r));
    std::string digits = "METAR LTAC 111250Z " + std::string(40, '9') + " 9999 SKC";
    TEST_ASSERT_TRUE(Metar::decode(digits.c_str(), r));
    TEST_ASSERT_EQUAL(10000, r.visM);
    TEST_ASSERT_EQUAL(N, r.windSpdKt);
    // Pseudo-random bytes must never crash or read past the end.
    unsigned seed = 12345;
    char junk[256];
    for (int k = 0; k < 500; k++) {
        size_t n = 1 + (seed % 255);
        for (size_t i = 0; i < n; i++) {
            seed = seed * 1103515245u + 12345u;
            junk[i] = char(1 + (seed >> 16) % 255);
        }
        junk[n] = 0;
        Metar::decode(junk, r);
        Metar::category(r);
        char b[40];
        Metar::formatWind(r, b, sizeof b);
        Metar::formatClouds(r, b, sizeof b);
    }
    // Input over 255 characters is capped, not overrun.
    std::string big = std::string("METAR LTAC 111250Z 27007KT 9999 ") + std::string(600, 'B') + " Q1020";
    TEST_ASSERT_TRUE(Metar::decode(big.c_str(), r));
    TEST_ASSERT_EQUAL(N, r.qnhHpa);   // beyond the cap
}

int main()
{
    UNITY_BEGIN();
    RUN_TEST(test_corpus);
    RUN_TEST(test_corpus_details);
    RUN_TEST(test_wind);
    RUN_TEST(test_visibility);
    RUN_TEST(test_clouds_and_ceiling);
    RUN_TEST(test_temperature_pressure_kind);
    RUN_TEST(test_category_boundaries);
    RUN_TEST(test_resolve_obs_time);
    RUN_TEST(test_is_stale);
    RUN_TEST(test_format_helpers);
    RUN_TEST(test_readable_text);
    RUN_TEST(test_pick_newest);
    RUN_TEST(test_robustness);
    return UNITY_END();
}
