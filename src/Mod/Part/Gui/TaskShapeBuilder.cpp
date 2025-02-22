/***************************************************************************
 *   Copyright (c) 2011 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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
# include <TopExp_Explorer.hxx>
# include <TopTools_IndexedMapOfShape.hxx>
# include <QButtonGroup>
# include <QMessageBox>
# include <QTextStream>
#endif

#include "ui_TaskShapeBuilder.h"
#include "TaskShapeBuilder.h"
#include "ViewProviderExt.h"

#include <Gui/Application.h>
#include <Gui/BitmapFactory.h>
#include <Gui/Document.h>
#include <Gui/Selection.h>
#include <Gui/SelectionFilter.h>

#include <Base/Console.h>
#include <Base/Interpreter.h>
#include <App/Application.h>
#include <App/Document.h>
#include <App/DocumentObject.h>
#include <Mod/Part/App/PartFeature.h>


using namespace PartGui;

namespace PartGui {
    class ShapeSelection : public Gui::SelectionFilterGate
    {
    public:
        enum Type {VERTEX, EDGE, FACE, ALL};
        Type mode;
        ShapeSelection()
            : Gui::SelectionFilterGate((Gui::SelectionFilter*)0), mode(ALL)
        {
        }
        void setMode(Type mode)
        {
            this->mode = mode;
        }
        bool allow(App::Document*, App::DocumentObject* obj, const char*sSubName)
        {
            if (!obj || !obj->isDerivedFrom(Part::Feature::getClassTypeId()))
                return false;
            if (!sSubName || sSubName[0] == '\0')
                return (mode == ALL);
            std::string element(sSubName);
            switch (mode) {
            case VERTEX:
                return element.substr(0,6) == "Vertex";
            case EDGE:
                return element.substr(0,4) == "Edge";
            case FACE:
                return element.substr(0,4) == "Face";
            default:
                return true;
            }
        }
    };
}

class ShapeBuilderWidget::Private
{
public:
    Ui_TaskShapeBuilder ui;
    QButtonGroup bg;
    ShapeSelection* gate;
    Private()
    {
    }
    ~Private()
    {
    }
};

/* TRANSLATOR PartGui::ShapeBuilderWidget */

ShapeBuilderWidget::ShapeBuilderWidget(QWidget* parent)
  : d(new Private())
{
    Gui::Application::Instance->runPythonCode("from FreeCAD import Base");
    Gui::Application::Instance->runPythonCode("import Part");

    d->ui.setupUi(this);
    d->ui.label->setText(QString());
    d->bg.addButton(d->ui.radioButtonEdge, 0);
    d->bg.addButton(d->ui.radioButtonFace, 1);
    d->bg.addButton(d->ui.radioButtonShell, 2);
    d->bg.addButton(d->ui.radioButtonSolid, 3);
    d->bg.setExclusive(true);

    connect(&d->bg, SIGNAL(buttonClicked(int)),
            this, SLOT(switchMode(int)));

    d->gate = new ShapeSelection();
    Gui::Selection().addSelectionGate(d->gate);

    d->bg.button(0)->setChecked(true);
    switchMode(0);
}

ShapeBuilderWidget::~ShapeBuilderWidget()
{
    Gui::Selection().rmvSelectionGate();
    delete d;
}

void ShapeBuilderWidget::on_createButton_clicked()
{
    int mode = d->bg.checkedId();
    Gui::Document* doc = Gui::Application::Instance->activeDocument();
    if (!doc) return;

    try {
        if (mode == 0) {
            createEdge();
        }
        else if (mode == 1) {
            createFace();
        }
        else if (mode == 2) {
            createShell();
        }
        else if (mode == 3) {
            createSolid();
        }
        doc->getDocument()->recompute();
    }
    catch (const Base::Exception& e) {
        Base::Console().Error("%s\n", e.what());
    }
}

void ShapeBuilderWidget::createEdge()
{
    Gui::SelectionFilter vertexFilter  ("SELECT Part::Feature SUBELEMENT Vertex COUNT 2");
    bool matchVertex = vertexFilter.match();
    if (!matchVertex) {
        QMessageBox::critical(this, tr("Wrong selection"), tr("Select two vertices"));
        return;
    }

    std::vector<Gui::SelectionObject> sel = vertexFilter.Result[0];
    std::vector<QString> elements;
    std::vector<Gui::SelectionObject>::iterator it;
    std::vector<std::string>::const_iterator jt;
    for (it=sel.begin();it!=sel.end();++it) {
        for (jt=it->getSubNames().begin();jt!=it->getSubNames().end();++jt) {
            QString line;
            QTextStream str(&line);
            str << "App.ActiveDocument." << it->getFeatName() << ".Shape." << jt->c_str() << ".Point";
            elements.push_back(line);
        }
    }

    // should actually never happen
    if (elements.size() != 2) {
        QMessageBox::critical(this, tr("Wrong selection"), tr("Select two vertices"));
        return;
    }

    QString cmd;
    cmd = QString::fromAscii(
        "_=Part.makeLine(%1, %2)\n"
        "if _.isNull(): raise Exception('Failed to create edge')\n"
        "App.ActiveDocument.addObject('Part::Feature','Edge').Shape=_\n"
        "del _\n"
    ).arg(elements[0]).arg(elements[1]);

    Gui::Application::Instance->activeDocument()->openCommand("Edge");
    Gui::Application::Instance->runPythonCode((const char*)cmd.toAscii(), false, false);
    Gui::Application::Instance->activeDocument()->commitCommand();
}

void ShapeBuilderWidget::createFace()
{
    Gui::SelectionFilter edgeFilter  ("SELECT Part::Feature SUBELEMENT Edge COUNT 3..");
    bool matchEdge = edgeFilter.match();
    if (!matchEdge) {
        QMessageBox::critical(this, tr("Wrong selection"), tr("Select three or more edges"));
        return;
    }

    std::vector<Gui::SelectionObject> sel = edgeFilter.Result[0];
    std::vector<Gui::SelectionObject>::iterator it;
    std::vector<std::string>::const_iterator jt;

    QString list;
    QTextStream str(&list);
    str << "[";
    for (it=sel.begin();it!=sel.end();++it) {
        for (jt=it->getSubNames().begin();jt!=it->getSubNames().end();++jt) {
            str << "App.ActiveDocument." << it->getFeatName() << ".Shape." << jt->c_str() << ", ";
        }
    }
    str << "]";

    QString cmd;
    if (d->ui.checkPlanar->isChecked()) {
        cmd = QString::fromAscii(
            "_=Part.Face(Part.Wire(Part.__sortEdges__(%1)))\n"
            "if _.isNull(): raise Exception('Failed to create face')\n"
            "App.ActiveDocument.addObject('Part::Feature','Face').Shape=_\n"
            "del _\n"
        ).arg(list);
    }
    else {
        cmd = QString::fromAscii(
            "_=Part.makeFilledFace(Part.__sortEdges__(%1))\n"
            "if _.isNull(): raise Exception('Failed to create face')\n"
            "App.ActiveDocument.addObject('Part::Feature','Face').Shape=_\n"
            "del _\n"
        ).arg(list);
    }

    Gui::Application::Instance->activeDocument()->openCommand("Face");
    Gui::Application::Instance->runPythonCode((const char*)cmd.toAscii(), false, false);
    Gui::Application::Instance->activeDocument()->commitCommand();
}

void ShapeBuilderWidget::createShell()
{
    Gui::SelectionFilter faceFilter  ("SELECT Part::Feature SUBELEMENT Face COUNT 2..");
    bool matchFace = faceFilter.match();
    if (!matchFace) {
        QMessageBox::critical(this, tr("Wrong selection"), tr("Select two or more faces"));
        return;
    }

    std::vector<Gui::SelectionObject> sel = faceFilter.Result[0];
    std::vector<Gui::SelectionObject>::iterator it;
    std::vector<std::string>::const_iterator jt;

    QString list;
    QTextStream str(&list);
    if (d->ui.checkFaces->isChecked()) {
        std::set<App::DocumentObject*> obj;
        for (it=sel.begin();it!=sel.end();++it)
            obj.insert(it->getObject());
        str << "[]";
        for (std::set<App::DocumentObject*>::iterator it = obj.begin(); it != obj.end(); ++it) {
            str << "+ App.ActiveDocument." << (*it)->getNameInDocument() << ".Shape.Faces";
        }
    }
    else {
        str << "[";
        for (it=sel.begin();it!=sel.end();++it) {
            for (jt=it->getSubNames().begin();jt!=it->getSubNames().end();++jt) {
                str << "App.ActiveDocument." << it->getFeatName() << ".Shape." << jt->c_str() << ", ";
            }
        }
        str << "]";
    }

    QString cmd;
    cmd = QString::fromAscii(
        "_=Part.Shell(%1)\n"
        "if _.isNull(): raise Exception('Failed to create shell')\n"
        "App.ActiveDocument.addObject('Part::Feature','Shell').Shape=_.removeSplitter()\n"
        "del _\n"
    ).arg(list);

    Gui::Application::Instance->activeDocument()->openCommand("Shell");
    Gui::Application::Instance->runPythonCode((const char*)cmd.toAscii(), false, false);
    Gui::Application::Instance->activeDocument()->commitCommand();
}

void ShapeBuilderWidget::createSolid()
{
    Gui::SelectionFilter partFilter  ("SELECT Part::Feature COUNT 1");
    bool matchPart = partFilter.match();
    if (!matchPart) {
        QMessageBox::critical(this, tr("Wrong selection"), tr("Select only one part object"));
        return;
    }

    QString line;
    QTextStream str(&line);

    std::vector<Gui::SelectionObject> sel = partFilter.Result[0];
    std::vector<Gui::SelectionObject>::iterator it;
    for (it=sel.begin();it!=sel.end();++it) {
        str << "App.ActiveDocument." << it->getFeatName() << ".Shape";
        break;
    }

    QString cmd;
    cmd = QString::fromAscii(
        "shell=%1\n"
        "if shell.ShapeType != 'Shell': raise Exception('Part object is not a shell')\n"
        "_=Part.Solid(shell)\n"
        "if _.isNull(): raise Exception('Failed to create solid')\n"
        "App.ActiveDocument.addObject('Part::Feature','Solid').Shape=_.removeSplitter()\n"
        "del _\n"
    ).arg(line);

    Gui::Application::Instance->activeDocument()->openCommand("Solid");
    Gui::Application::Instance->runPythonCode((const char*)cmd.toAscii(), false, false);
    Gui::Application::Instance->activeDocument()->commitCommand();
}

void ShapeBuilderWidget::switchMode(int mode)
{
    Gui::Selection().clearSelection();
    if (mode == 0) {
        d->gate->setMode(ShapeSelection::VERTEX);
        d->ui.label->setText(tr("Select two vertices to create an edge"));
        d->ui.checkPlanar->setEnabled(false);
        d->ui.checkFaces->setEnabled(false);
    }
    else if (mode == 1) {
        d->gate->setMode(ShapeSelection::EDGE);
        d->ui.label->setText(tr("Select a closed set of edges"));
        d->ui.checkPlanar->setEnabled(true);
        d->ui.checkFaces->setEnabled(false);
    }
    else if (mode == 2) {
        d->gate->setMode(ShapeSelection::FACE);
        d->ui.label->setText(tr("Select adjacent faces"));
        d->ui.checkPlanar->setEnabled(false);
        d->ui.checkFaces->setEnabled(true);
    }
    else {
        d->gate->setMode(ShapeSelection::ALL);
        d->ui.label->setText(tr("All shape types can be selected"));
        d->ui.checkPlanar->setEnabled(false);
        d->ui.checkFaces->setEnabled(false);
    }
}

bool ShapeBuilderWidget::accept()
{
    return true;
}

bool ShapeBuilderWidget::reject()
{
    return true;
}

void ShapeBuilderWidget::changeEvent(QEvent *e)
{
    QWidget::changeEvent(e);
    if (e->type() == QEvent::LanguageChange) {
        d->ui.retranslateUi(this);
    }
}


/* TRANSLATOR PartGui::TaskShapeBuilder */

TaskShapeBuilder::TaskShapeBuilder()
{
    widget = new ShapeBuilderWidget();
    taskbox = new Gui::TaskView::TaskBox(
        Gui::BitmapFactory().pixmap("Part_Shapebuilder"),
        widget->windowTitle(), true, 0);
    taskbox->groupLayout()->addWidget(widget);
    Content.push_back(taskbox);
}

TaskShapeBuilder::~TaskShapeBuilder()
{
}

void TaskShapeBuilder::open()
{
}

void TaskShapeBuilder::clicked(int)
{
}

bool TaskShapeBuilder::accept()
{
    return widget->accept();
}

bool TaskShapeBuilder::reject()
{
    return widget->reject();
}

#include "moc_TaskShapeBuilder.cpp"
