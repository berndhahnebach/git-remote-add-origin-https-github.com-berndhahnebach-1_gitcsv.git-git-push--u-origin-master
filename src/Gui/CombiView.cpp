/***************************************************************************
 *   Copyright (c) 2009 J�rgen Riegel <juergen.riegel@web.de>              *
 *                                                                         *
 *   This file is part of the FreeCAD CAx development system.              *
 *                                                                         *
 *   This library is free software; you can redistribute it and/or         *
 *   modify it under the terms of the GNU Library General Public           *
 *   License as published by the Free Software Foundation; either          *
 *   version 2 of the License, or (at your option) any later version.      *
 *                                                                         *
 *   This library  is distributed in the hope that it will be useful,      *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU Library General Public License for more details.                  *
 *                                                                         *
 *   You should have received a copy of the GNU Library General Public     *
 *   License along with this library; see the file COPYING.LIB. If not,    *
 *   write to the Free Software Foundation, Inc., 59 Temple Place,         *
 *   Suite 330, Boston, MA  02111-1307, USA                                *
 *                                                                         *
 ***************************************************************************/


#include "PreCompiled.h"

/// Here the FreeCAD includes sorted by Base,App,Gui......

#include "CombiView.h"
#include "BitmapFactory.h"
#include "iisTaskPanel/include/iisTaskPanel"
#include "PropertyView.h"
#include "Application.h"
#include "Document.h"
#include "Tree.h"
#include "TaskView/TaskView.h"
#include "propertyeditor/PropertyEditor.h"

using namespace Gui;
using namespace Gui::DockWnd;


/* TRANSLATOR Gui::DockWnd::CombiView */

CombiView::CombiView(Gui::Document* pcDocument, QWidget *parent)
  : DockWindow(pcDocument,parent), oldTabIndex(0)
{
    setWindowTitle(tr("CombiView"));

    QGridLayout* pLayout = new QGridLayout(this); 
    pLayout->setSpacing( 0 );
    pLayout->setMargin ( 0 );

    // tabs to switch between Tree/Properties and TaskPanel
    tabs = new QTabWidget ();
    tabs->setObjectName(QString::fromUtf8("combiTab"));
    tabs->setTabPosition(QTabWidget::North);
    //tabs->setTabShape(QTabWidget::Triangular);
    pLayout->addWidget( tabs, 0, 0 );

    // splitter between tree and property view
    QSplitter *splitter = new QSplitter();
    splitter->setOrientation(Qt::Vertical);

    // tree widget
    tree =  new TreeWidget(this);
    //tree->setRootIsDecorated(false);
    splitter->addWidget(tree);

    // property view
    prop = new PropertyView(this);
    splitter->addWidget(prop);

    tabs->addTab(splitter,trUtf8("Project"));

    // task panel
    taskPanel = new Gui::TaskView::TaskView(this);
    tabs->addTab(taskPanel, trUtf8("Tasks"));
}

CombiView::~CombiView()
{
}

void CombiView::showDialog(Gui::TaskView::TaskDialog *dlg)
{
    // switch to the TaskView tab
    oldTabIndex = tabs->currentIndex();
    tabs->setCurrentIndex(1);
    // set the dialog
    taskPanel->showDialog(dlg);
}

void CombiView::closeDialog()
{
    // close the dialog
    taskPanel->removeDialog();
}

void CombiView::closedDialog()
{
    // dialog has been closed
    tabs->setCurrentIndex(oldTabIndex);
}

void CombiView::showTreeView()
{
    // switch to the tree view
    tabs->setCurrentIndex(0);
}

void CombiView::showTaskView()
{
    // switch to the task view
    tabs->setCurrentIndex(1);
}

void CombiView::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::LanguageChange) {
        tabs->setTabText(0, trUtf8("Project"));
        tabs->setTabText(1, trUtf8("Tasks"));
    }

    DockWindow::changeEvent(e);
}


#include "moc_CombiView.cpp"
