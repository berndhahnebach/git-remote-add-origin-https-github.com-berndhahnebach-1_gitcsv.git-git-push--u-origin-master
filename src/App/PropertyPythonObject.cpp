/***************************************************************************
 *   Copyright (c) 2009 Werner Mayer <wmayer[at]users.sourceforge.net>     *
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

#include "PropertyPythonObject.h"
#include "DocumentObjectPy.h"
#include "DocumentObject.h"
#include <Base/Base64.h>
#include <Base/Writer.h>
#include <Base/Reader.h>
#include <Base/Console.h>
#include <Base/Interpreter.h>
#include <iostream>

using namespace App;


TYPESYSTEM_SOURCE(App::PropertyPythonObject , App::Property);

PropertyPythonObject::PropertyPythonObject()
{
}

PropertyPythonObject::~PropertyPythonObject()
{
    // this is needed because the release of the pickled object may need the
    // GIL. Thus, we grab the GIL and replace the pickled with an empty object
    Base::PyGILStateLocker lock;
    this->object = Py::Object();
}

void PropertyPythonObject::setValue(Py::Object o)
{
    aboutToSetValue();
    this->object = o;
    hasSetValue();
}

Py::Object PropertyPythonObject::getValue() const
{
    return object;
}

PyObject *PropertyPythonObject::getPyObject(void)
{
    return Py::new_reference_to(this->object);
}

void PropertyPythonObject::setPyObject(PyObject * obj)
{
    aboutToSetValue();
    this->object = obj;
    hasSetValue();
}

std::string PropertyPythonObject::toString() const
{
    std::string repr;
    Base::PyGILStateLocker lock;
    try {
        Py::Module pickle(PyImport_ImportModule("cPickle"),true);
        Py::Callable method(pickle.getAttr(std::string("dumps")));
        Py::Tuple args(1);
        args.setItem(0, this->object);
        Py::Object res = method.apply(args);
        Py::String str(res);
        repr = str.as_std_string();
    }
    catch (Py::Exception&) {
        Base::PyException e; // extract the Python error text
        Base::Console().Warning("PropertyPythonObject::toString: %s\n", e.what());
    }

    return repr;
}

void PropertyPythonObject::fromString(const std::string& repr)
{
    Base::PyGILStateLocker lock;
    try {
        Py::Module pickle(PyImport_ImportModule("cPickle"),true);
        Py::Callable method(pickle.getAttr(std::string("loads")));
        Py::Tuple args(1);
        args.setItem(0, Py::String(repr));
        Py::Object res = method.apply(args);
        this->object = res;
    }
    catch (Py::Exception&) {
        Base::PyException e; // extract the Python error text
        Base::Console().Warning("PropertyPythonObject::fromString: %s\n", e.what());
    }
}

std::string PropertyPythonObject::encodeValue(const std::string& str) const
{
    std::string tmp;
    for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
        if (*it == '<')
            tmp += "&lt;";
        else if (*it == '"')
            tmp += "&quot;";
        else if (*it == '&')
            tmp += "&amp;";
        else if (*it == '>')
            tmp += "&gt";
        else if (*it == '\n')
            tmp += "\\n";
        else
            tmp += *it;
    }

    return tmp;
}

std::string PropertyPythonObject::decodeValue(const std::string& str) const
{
    std::string tmp;
    for (std::string::const_iterator it = str.begin(); it != str.end(); ++it) {
        if (*it == '\\') {
            ++it;
            if (it != str.end() && *it == 'n') {
                tmp += '\n';
            }
        }
        else
            tmp += *it;
    }

    return tmp;
}

void PropertyPythonObject::saveObject(Base::Writer &writer) const
{
    Base::PyGILStateLocker lock;
    try {
        PropertyContainer* parent = this->getContainer();
        if (parent->isDerivedFrom(Base::Type::fromName("App::DocumentObject"))) {
            if (this->object.hasAttr("__object__")) {
                writer.Stream() << " object=\"yes\"";
            }
        }
        if (parent->isDerivedFrom(Base::Type::fromName("Gui::ViewProvider"))) {
            if (this->object.hasAttr("__vobject__")) {
                writer.Stream() << " vobject=\"yes\"";
            }
        }
    }
    catch (Py::Exception& e) {
        e.clear();
    }
}

void PropertyPythonObject::restoreObject(Base::XMLReader &reader)
{
    Base::PyGILStateLocker lock;
    try {
        PropertyContainer* parent = this->getContainer();
        if (reader.hasAttribute("object")) {
            if (strcmp(reader.getAttribute("object"),"yes") == 0) {
                Py::Object obj = Py::asObject(parent->getPyObject());
                this->object.setAttr("__object__", obj);
            }
        }
        if (reader.hasAttribute("vobject")) {
            if (strcmp(reader.getAttribute("vobject"),"yes") == 0) {
                Py::Object obj = Py::asObject(parent->getPyObject());
                this->object.setAttr("__vobject__", obj);
            }
        }
    }
    catch (Py::Exception& e) {
        e.clear();
    }
    catch (const Base::Exception& e) {
        Base::Console().Error("%s\n",e.what());
    }
    catch (...) {
        Base::Console().Error("Critical error in PropertyPythonObject::restoreObject\n");
    }
}

void PropertyPythonObject::Save (Base::Writer &writer) const
{
    //if (writer.isForceXML()) {
        std::string repr = this->toString();
        repr = Base::base64_encode((const unsigned char*)repr.c_str(), repr.size());
        std::string val = /*encodeValue*/(repr);
        writer.Stream() << writer.ind() << "<Python value=\"" << val
                        <<"\" encoded=\"yes\"";
        saveObject(writer);
        writer.Stream() << "/>" << std::endl;
    //}
    //else {
    //    writer.Stream() << writer.ind() << "<Python file=\"" << 
    //    writer.addFile("pickle", this) << "\"/>" << std::endl;
    //}
}

void PropertyPythonObject::Restore(Base::XMLReader &reader)
{
    reader.readElement("Python");
    if (reader.hasAttribute("file")) {
        std::string file(reader.getAttribute("file"));
        reader.addFile(file.c_str(),this);
    }
    else {
        std::string buffer = reader.getAttribute("value");
        if (reader.hasAttribute("encoded") &&
            strcmp(reader.getAttribute("encoded"),"yes") == 0) {
            buffer = Base::base64_decode(buffer);
        }
        else {
            buffer = decodeValue(buffer);
        }

        aboutToSetValue();
        this->fromString(buffer);
        restoreObject(reader);
        hasSetValue();
    }
}

void PropertyPythonObject::SaveDocFile (Base::Writer &writer) const
{
    std::string buffer = this->toString();
    for (std::string::iterator it = buffer.begin(); it != buffer.end(); ++it)
        writer.Stream().put(*it);
}

void PropertyPythonObject::RestoreDocFile(Base::Reader &reader)
{
    aboutToSetValue();
    std::string buffer;
    char c;
    while (reader.get(c)) {
        buffer.push_back(c);
    }
    this->fromString(buffer);
    hasSetValue();
}

unsigned int PropertyPythonObject::getMemSize (void) const
{
    return sizeof(Py::Object);
}

Property *PropertyPythonObject::Copy(void) const
{
    PropertyPythonObject *p = new PropertyPythonObject();
    p->object = this->object;
    return p;
}

void PropertyPythonObject::Paste(const Property &from)
{
    if (from.getTypeId() == PropertyPythonObject::getClassTypeId()) {
        aboutToSetValue();
        this->object = static_cast<const PropertyPythonObject&>(from).object;
        hasSetValue();
    }
}
