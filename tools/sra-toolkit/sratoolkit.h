/*===========================================================================
*
*                            PUBLIC DOMAIN NOTICE
*               National Center for Biotechnology Information
*
*  This software/database is a "United States Government Work" under the
*  terms of the United States Copyright Act.  It was written as part of
*  the author's official duties as a United States Government employee and
*  thus cannot be copyrighted.  This software/database is freely available
*  to the public for use. The National Library of Medicine and the U.S.
*  Government have not placed any restriction on its use or reproduction.
*
*  Although all reasonable efforts have been taken to ensure the accuracy
*  and reliability of the software and data, the NLM and the U.S.
*  Government do not and cannot warrant the performance or results that
*  may be obtained by using this software or data. The NLM and the U.S.
*  Government disclaim all warranties, express or implied, including
*  warranties of performance, merchantability or fitness for any particular
*  purpose.
*
*  Please cite the author in any work or product based on this material.
*
* ===========================================================================
*
*/

#ifndef SRATOOLKIT_H
#define SRATOOLKIT_H

#include <QMainWindow>

QT_BEGIN_NAMESPACE
class QHBoxLayout;
class QToolButton;
QT_END_NAMESPACE

class SRAToolBar;
class SRAToolView;

struct KConfig;

class SRAToolkit : public QMainWindow
{
    Q_OBJECT

public:
    SRAToolkit ( const QRect &avail_geometry, QWidget *parent = 0 );
    ~SRAToolkit ();

private slots:
    void expand ( bool );

private:

    void init ();
    void init_menubar ();
    void init_view ();
    void paintEvent ( QPaintEvent * );

    QHBoxLayout *mainLayout;
    QWidget *mainWidget;

    KConfig *config;

    SRAToolBar *toolBar;
    SRAToolView *toolView;

};

#endif // SRATOOLKIT_H
