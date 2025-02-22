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

#ifndef _PreComp_
#endif

#include "TaskDlgEditSketch.h"
#include "ViewProviderSketch.h"
#include <Gui/Command.h>

using namespace SketcherGui;


//**************************************************************************
//**************************************************************************
// TaskDialog
//++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

TaskDlgEditSketch::TaskDlgEditSketch(ViewProviderSketch *sketchView)
    : TaskDialog(),sketchView(sketchView)
{
    assert(sketchView);
    Constraints  = new TaskSketcherConstrains(sketchView);
    General  = new TaskSketcherGeneral(sketchView);
    Messages  = new TaskSketcherMessages(sketchView);
    
    Content.push_back(Messages);
    Content.push_back(General);
    Content.push_back(Constraints);
}

TaskDlgEditSketch::~TaskDlgEditSketch()
{

}

//==== calls from the TaskView ===============================================================


void TaskDlgEditSketch::open()
{

}

void TaskDlgEditSketch::clicked(int)
{
    
}

bool TaskDlgEditSketch::accept()
{
    return true;
}

bool TaskDlgEditSketch::reject()
{
//    Gui::Command::openCommand("Sketch changed");
    Gui::Command::doCommand(Gui::Command::Gui,"Gui.activeDocument().resetEdit()");
    Gui::Command::doCommand(Gui::Command::Doc,"App.ActiveDocument.recompute()");
//    Gui::Command::commitCommand();

    return true;
}

void TaskDlgEditSketch::helpRequested()
{

}


#include "moc_TaskDlgEditSketch.cpp"
