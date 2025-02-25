# Copyright (C) 2022 The Qt Company Ltd.
# SPDX-License-Identifier: LicenseRef-Qt-Commercial OR GPL-3.0-only WITH Qt-GPL-exception-1.0
from __future__ import annotations

'''Test data for QImage'''


xpm = [
    "27 22 206 2",
    "   c None",
    ".  c #FEFEFE",
    "+  c #FFFFFF",
    "@  c #F9F9F9",
    "#  c #ECECEC",
    "$  c #D5D5D5",
    "%  c #A0A0A0",
    "&  c #767676",
    "*  c #525252",
    "=  c #484848",
    "-  c #4E4E4E",
    ";  c #555555",
    ">  c #545454",
    ",  c #5A5A5A",
    "'  c #4B4B4B",
    ")  c #4A4A4A",
    "!  c #4F4F4F",
    "~  c #585858",
    "{  c #515151",
    "]  c #4C4C4C",
    "^  c #B1B1B1",
    "/  c #FCFCFC",
    "(  c #FDFDFD",
    "_  c #C1C1C1",
    ":  c #848484",
    "<  c #616161",
    "[  c #5E5E5E",
    "}  c #CECECE",
    "|  c #E2E2E2",
    "1  c #E4E4E4",
    "2  c #DFDFDF",
    "3  c #D2D2D2",
    "4  c #D8D8D8",
    "5  c #D4D4D4",
    "6  c #E6E6E6",
    "7  c #F1F1F1",
    "8  c #838383",
    "9  c #8E8E8E",
    "0  c #8F8F8F",
    "a  c #CBCBCB",
    "b  c #CCCCCC",
    "c  c #E9E9E9",
    "d  c #F2F2F2",
    "e  c #EDEDED",
    "f  c #B5B5B5",
    "g  c #A6A6A6",
    "h  c #ABABAB",
    "i  c #BBBBBB",
    "j  c #B0B0B0",
    "k  c #EAEAEA",
    "l  c #6C6C6C",
    "m  c #BCBCBC",
    "n  c #F5F5F5",
    "o  c #FAFAFA",
    "p  c #B6B6B6",
    "q  c #F3F3F3",
    "r  c #CFCFCF",
    "s  c #FBFBFB",
    "t  c #CDCDCD",
    "u  c #DDDDDD",
    "v  c #999999",
    "w  c #F0F0F0",
    "x  c #2B2B2B",
    "y  c #C3C3C3",
    "z  c #A4A4A4",
    "A  c #D7D7D7",
    "B  c #E7E7E7",
    "C  c #6E6E6E",
    "D  c #9D9D9D",
    "E  c #BABABA",
    "F  c #AEAEAE",
    "G  c #898989",
    "H  c #646464",
    "I  c #BDBDBD",
    "J  c #CACACA",
    "K  c #2A2A2A",
    "L  c #212121",
    "M  c #B7B7B7",
    "N  c #F4F4F4",
    "O  c #737373",
    "P  c #828282",
    "Q  c #4D4D4D",
    "R  c #000000",
    "S  c #151515",
    "T  c #B2B2B2",
    "U  c #D6D6D6",
    "V  c #D3D3D3",
    "W  c #2F2F2F",
    "X  c #636363",
    "Y  c #A1A1A1",
    "Z  c #BFBFBF",
    "`  c #E0E0E0",
    " . c #6A6A6A",
    ".. c #050505",
    "+. c #A3A3A3",
    "@. c #202020",
    "#. c #5F5F5F",
    "$. c #B9B9B9",
    "%. c #C7C7C7",
    "&. c #D0D0D0",
    "*. c #3E3E3E",
    "=. c #666666",
    "-. c #DBDBDB",
    ";. c #424242",
    ">. c #C2C2C2",
    ",. c #1A1A1A",
    "'. c #2C2C2C",
    "). c #F6F6F6",
    "!. c #AAAAAA",
    "~. c #DCDCDC",
    "{. c #2D2D2D",
    "]. c #2E2E2E",
    "^. c #A7A7A7",
    "/. c #656565",
    "(. c #333333",
    "_. c #464646",
    ":. c #C4C4C4",
    "<. c #B8B8B8",
    "[. c #292929",
    "}. c #979797",
    "|. c #EFEFEF",
    "1. c #909090",
    "2. c #8A8A8A",
    "3. c #575757",
    "4. c #676767",
    "5. c #C5C5C5",
    "6. c #7A7A7A",
    "7. c #797979",
    "8. c #989898",
    "9. c #EEEEEE",
    "0. c #707070",
    "a. c #C8C8C8",
    "b. c #111111",
    "c. c #AFAFAF",
    "d. c #474747",
    "e. c #565656",
    "f. c #E3E3E3",
    "g. c #494949",
    "h. c #5B5B5B",
    "i. c #222222",
    "j. c #353535",
    "k. c #D9D9D9",
    "l. c #0A0A0A",
    "m. c #858585",
    "n. c #E5E5E5",
    "o. c #0E0E0E",
    "p. c #9A9A9A",
    "q. c #6F6F6F",
    "r. c #868686",
    "s. c #060606",
    "t. c #1E1E1E",
    "u. c #E8E8E8",
    "v. c #A5A5A5",
    "w. c #0D0D0D",
    "x. c #030303",
    "y. c #272727",
    "z. c #131313",
    "A. c #1F1F1F",
    "B. c #757575",
    "C. c #F7F7F7",
    "D. c #414141",
    "E. c #080808",
    "F. c #6B6B6B",
    "G. c #313131",
    "H. c #C0C0C0",
    "I. c #C9C9C9",
    "J. c #0B0B0B",
    "K. c #232323",
    "L. c #434343",
    "M. c #3D3D3D",
    "N. c #282828",
    "O. c #7C7C7C",
    "P. c #252525",
    "Q. c #3A3A3A",
    "R. c #F8F8F8",
    "S. c #1B1B1B",
    "T. c #949494",
    "U. c #3B3B3B",
    "V. c #242424",
    "W. c #383838",
    "X. c #6D6D6D",
    "Y. c #818181",
    "Z. c #939393",
    "`. c #9E9E9E",
    " + c #929292",
    ".+ c #7D7D7D",
    "++ c #ADADAD",
    "@+ c #DADADA",
    "#+ c #919191",
    "$+ c #E1E1E1",
    "%+ c #BEBEBE",
    "&+ c #ACACAC",
    "*+ c #9C9C9C",
    "=+ c #B3B3B3",
    "-+ c #808080",
    ";+ c #A8A8A8",
    ">+ c #393939",
    ",+ c #747474",
    "'+ c #7F7F7F",
    ")+ c #D1D1D1",
    "!+ c #606060",
    "~+ c #5C5C5C",
    "{+ c #686868",
    "]+ c #7E7E7E",
    "^+ c #787878",
    "/+ c #595959",
    ". . . + @ # $ % & * = - ; > , ' ) ! ~ { ] ^ / . . + + ",
    ". ( + _ : < [ & } | 1 2 $ 3 4 5 3 6 7 + + 8 9 + . + . ",
    ". + 0 9 a ( 3 a b c d e c f g h i g j $ k + l m + . + ",
    "+ 2 8 n o p | ( q r s . # t + + + u ^ v e w + x + + + ",
    "+ y z . @ A k B 7 n + ( s | p 8 C D 2 E 4 + + F G + . ",
    "# H I $ J G K L - M N . 2 O P Q R R S T U s s V W j + ",
    "X Y Z @ o ` _ g  ...+.( 4 @.#.m G $.%.7 &.X *.=.-.;.&.",
    "Q >.C ,.'.} e + ).!.k + . + + . ~.{.> ].x f 7 ^./.k (.",
    "_.:.4 @ <.[.}.|.1.2.+ + + >.} 4 B + ( @ _ 3.4.5.6.r 7.",
    "3.8.9.~ 0.+ a.Q b.+ + c.d.#.=.$ |.b #.e.z ^ ; ^. .f.g.",
    "-.h.+ i.S M + # p j.% n 9.5.k.H l.m.V ^.n.o.M + M p.q.",
    "7 r.N s.1.R t.<.|.| u.v.~ w.x.E + s y.z.A.B.C.+ 5 D.q ",
    ").p.2 E.0.9 F.%.O {._ @.+ + i { [ i.G.H.P I.+ s q.} + ",
    ").p.6 J.R b.K.L.M.A.! b.g.K [.R M k + N.I + + >.O.+ . ",
    ").8.9.N.P...R R R R E.t.W n.+ Q.R.6 @.| + . + S.+ + . ",
    "n }.w T.U.B.<.i.@ Y + + U.+ c u V.= B B 7 u.W.c + . + ",
    "N T.# + }.X.Y.,.8.F.8 Z.[.`. +.+}.4 ++@+O.< ~.+ ( . + ",
    "d #+1 + _ ~.u.$+b $.y @+| $+%+I.&+k.h W +.9.+ ( . + . ",
    "w 0 |.*+. >.<.=+++++p a.p -+;+5.k.>+,+@ + . . + . + + ",
    "q '+9.R.^ I.t b %.I.)+4 $+n.I.,+ .|.+ . . . + . + + + ",
    ". p !+( + + + + + + E 0. .-+8.f.+ + . . + + . + + + + ",
    ". ( A ~+{+]+^+l > /+D f.c q . + . . + + . + + + + + + "
]
