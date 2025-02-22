/***************************************************************************
 *   Copyright (c) Riegel         <juergen.riegel@web.de>                  *
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
# include <xercesc/sax/SAXParseException.hpp>
# include <xercesc/sax/SAXException.hpp>
# include <xercesc/sax2/XMLReaderFactory.hpp>
# include <xercesc/sax2/SAX2XMLReader.hpp>
#endif

#include <locale>

/// Here the FreeCAD includes sorted by Base,App,Gui......
#include "Reader.h"
#include "Exception.h"
#include "Persistence.h"
#include "InputSource.h"
#include "Console.h"
#include "Sequencer.h"

#include <zipios++/zipios-config.h>
#include <zipios++/zipfile.h>
#include <zipios++/zipinputstream.h>
#include <zipios++/zipoutputstream.h>
#include <zipios++/meta-iostreams.h>

#include "XMLTools.h"

XERCES_CPP_NAMESPACE_USE

using namespace std;



// ---------------------------------------------------------------------------
//  Base::XMLReader: Constructors and Destructor
// ---------------------------------------------------------------------------

Base::XMLReader::XMLReader(const char* FileName, std::istream& str) 
  : DocumentSchema(0), Level(0), _File(FileName)
{
#ifdef _MSC_VER
    str.imbue(std::locale::empty());
#else
    //FIXME: Check whether this is correct
    str.imbue(std::locale::classic());
#endif

    // create the parser
    parser = XMLReaderFactory::createXMLReader();
    //parser->setFeature(XMLUni::fgSAX2CoreNameSpaces, false);
    //parser->setFeature(XMLUni::fgXercesSchema, false);
    //parser->setFeature(XMLUni::fgXercesSchemaFullChecking, false);
    //parser->setFeature(XMLUni::fgXercesIdentityConstraintChecking, false);
    //parser->setFeature(XMLUni::fgSAX2CoreNameSpacePrefixes, false);
    //parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
    //parser->setFeature(XMLUni::fgXercesDynamic, true);

    parser->setContentHandler(this);
    parser->setErrorHandler(this);

    try {
        StdInputSource file(str, _File.filePath().c_str());
        _valid = parser->parseFirst( file,token);
    }
    catch (const XMLException& toCatch) {
        char* message = XMLString::transcode(toCatch.getMessage());
        cerr << "Exception message is: \n"
             << message << "\n";
        XMLString::release(&message);
    }
    catch (const SAXParseException& toCatch) {
        char* message = XMLString::transcode(toCatch.getMessage());
        cerr << "Exception message is: \n"
             << message << "\n";
        XMLString::release(&message);
    }
#ifndef FC_DEBUG
    catch (...) {
        cerr << "Unexpected Exception \n";
    }
#endif
}

Base::XMLReader::~XMLReader()
{
    //  Delete the parser itself.  Must be done prior to calling Terminate, below.
    delete parser;
}

const char* Base::XMLReader::localName(void) const
{
    return LocalName.c_str();
}

unsigned int Base::XMLReader::getAttributeCount(void) const
{
    return (unsigned int)AttrMap.size();
}

long Base::XMLReader::getAttributeAsInteger(const char* AttrName) const
{
    AttrMapType::const_iterator pos = AttrMap.find(AttrName);

    if (pos != AttrMap.end())
        return atol(pos->second.c_str());
    else
        // wrong name, use hasAttribute if not shure!
        assert(0);

    return 0;
}

unsigned long Base::XMLReader::getAttributeAsUnsigned(const char* AttrName) const
{
    AttrMapType::const_iterator pos = AttrMap.find(AttrName);

    if (pos != AttrMap.end())
        return strtoul(pos->second.c_str(),0,10);
    else
        // wrong name, use hasAttribute if not shure!
        assert(0);

    return 0;
}

double Base::XMLReader::getAttributeAsFloat  (const char* AttrName) const
{
    AttrMapType::const_iterator pos = AttrMap.find(AttrName);

    if (pos != AttrMap.end())
        return atof(pos->second.c_str());
    else
        // wrong name, use hasAttribute if not shure!
        assert(0);

    return 0.0;
}

const char*  Base::XMLReader::getAttribute (const char* AttrName) const
{
    AttrMapType::const_iterator pos = AttrMap.find(AttrName);

    if (pos != AttrMap.end())
        return pos->second.c_str();
    else
        // wrong name, use hasAttribute if not shure!
        assert(0);

    return ""; 
}

bool Base::XMLReader::hasAttribute (const char* AttrName) const
{
    return AttrMap.find(AttrName) != AttrMap.end();
}

bool Base::XMLReader::read(void)
{
    ReadType = None;

    try {
        parser->parseNext(token);
    }
    catch (const XMLException& toCatch) {
#if 0
        char* message = XMLString::transcode(toCatch.getMessage());
        cerr << "Exception message is: \n"
             << message << "\n";
        XMLString::release(&message);
        return false;
#else
        char* message = XMLString::transcode(toCatch.getMessage());
        std::string what = message;
        XMLString::release(&message);
        throw Base::Exception(what);
#endif
    }
    catch (const SAXParseException& toCatch) {
#if 0
        char* message = XMLString::transcode(toCatch.getMessage());
        cerr << "Exception message is: \n"
             << message << "\n";
        XMLString::release(&message);
        return false;
#else
        char* message = XMLString::transcode(toCatch.getMessage());
        std::string what = message;
        XMLString::release(&message);
        throw Base::XMLParseException(what);
#endif
    }
    catch (...) {
#if 0
        cerr << "Unexpected Exception \n" ;
        return false;
#else
        throw Base::Exception("Unexpected XML exception");
#endif
    }

    return true;
}

void Base::XMLReader::readElement(const char* ElementName)
{
    bool ok;
    int currentLevel = Level;
    std::string currentName = LocalName;
    do {
        ok = read(); if (!ok) break;
        if (ReadType == EndElement && currentName == LocalName && currentLevel >= Level) {
            // we have reached the end of the element when calling this method
            // thus we must stop reading on.
            break;
        }
    } while ((ReadType != StartElement && ReadType != StartEndElement) ||
             (ElementName && LocalName != ElementName));
}

void Base::XMLReader::readEndElement(const char* ElementName)
{
    // if we are already at the end of the current element
    if (ReadType == EndElement && LocalName == ElementName)
        return;
    bool ok;
    do {
        ok = read(); if (!ok) break;
    } while (ReadType != EndElement || (ElementName && LocalName != ElementName));
}

void Base::XMLReader::readCharacters(void)
{
}

void Base::XMLReader::readFiles(zipios::ZipInputStream &zipstream) const
{
    // It's possible that not all objects inside the document could be created, e.g. if a module
    // is missing that would know these object types. So, there may be data files inside the zip
    // file that cannot be read. We simply ignore these files. 
    // On the other hand, however, it could happen that a file should be read that is not part of
    // the zip file. This happens e.g. if a document is written without GUI up but is read with GUI
    // up. In this case the associated GUI document asks for its file which is not part of the ZIP
    // file, then.
    // In either case it's guaranteed that the order of the files is kept.
    zipios::ConstEntryPointer entry;
    try {
        entry = zipstream.getNextEntry();
    }
    catch (const std::exception&) {
        // There is no further file at all. This can happen if the
        // project file was created without GUI
        return;
    }
    std::vector<FileEntry>::const_iterator it = FileList.begin();
    Base::SequencerLauncher seq("Importing project files...", FileList.size());
    while (entry->isValid() && it != FileList.end()) {
        std::vector<FileEntry>::const_iterator jt = it; 
        // Check if the current entry is registered, otherwise check the next registered files as soon as
        // both file names match
        while (jt != FileList.end() && entry->getName() != jt->FileName)
            ++jt;
        // If this condition is true both file names match and we can read-in the data, otherwise
        // no file name for the current entry in the zip was registered.
        if (jt != FileList.end()) {
            try {
                jt->Object->RestoreDocFile(zipstream);
            }
            catch(...) {
                // For any exception we just continue with the next file.
                // It doesn't matter if the last reader has read more or
                // less data than the file size would allow.
                // All what we need to do is to notify the user about the
                // failure.
                Base::Console().Error("Reading failed from embedded file: %s\n", entry->toString().c_str());
            }
            // Go to the next registered file name
            it = jt + 1;
        }

        seq.next();

        // In either case we must go to the next entry
        try {
            entry = zipstream.getNextEntry();
        }
        catch (const std::exception&) {
            // there is no further entry
            break;
        }
    }
}

const char *Base::XMLReader::addFile(const char* Name, Base::Persistence *Object)
{
    FileEntry temp;
    temp.FileName = Name;
    temp.Object = Object;

    FileList.push_back(temp);
    FileNames.push_back( temp.FileName );

    return Name;
}

const std::vector<std::string>& Base::XMLReader::getFilenames() const
{
    return FileNames;
}

bool Base::XMLReader::isRegistered(Base::Persistence *Object) const
{
    if (Object) {
        for (std::vector<FileEntry>::const_iterator it = FileList.begin(); it != FileList.end(); ++it) {
            if (it->Object == Object)
                return true;
        }
    }

    return false;
}

// ---------------------------------------------------------------------------
//  Base::XMLReader: Implementation of the SAX DocumentHandler interface
// ---------------------------------------------------------------------------
void Base::XMLReader::startElement(const XMLCh* const /*uri*/, const XMLCh* const localname, const XMLCh* const /*qname*/, const XERCES_CPP_NAMESPACE_QUALIFIER Attributes& attrs)
{
    Level++; // new scope
    LocalName = StrX(localname).c_str();

    // saving attributes of the current scope, delete all previously stored ones
    AttrMap.clear();
    for (unsigned int i = 0; i < attrs.getLength(); i++) {
        AttrMap[StrX(attrs.getQName(i)).c_str()] = StrXUTF8(attrs.getValue(i)).c_str();
    }

    ReadType = StartElement;
}

void Base::XMLReader::endElement  (const XMLCh* const /*uri*/, const XMLCh *const localname, const XMLCh *const /*qname*/)
{
    Level--; // end of scope
    LocalName = StrX(localname).c_str();

    if (ReadType == StartElement)
        ReadType = StartEndElement;
    else
        ReadType = EndElement;
}


void Base::XMLReader::characters(const   XMLCh* const chars, const unsigned int length)
{
    Characters = StrX(chars).c_str();
    ReadType = Chars;
    CharacterCount += length;
}

void Base::XMLReader::ignorableWhitespace( const   XMLCh* const /*chars*/, const unsigned int /*length*/)
{
    //fSpaceCount += length;
}

void Base::XMLReader::resetDocument()
{
    //fAttrCount = 0;
    //fCharacterCount = 0;
    //fElementCount = 0;
    //fSpaceCount = 0;
}


// ---------------------------------------------------------------------------
//  Base::XMLReader: Overrides of the SAX ErrorHandler interface
// ---------------------------------------------------------------------------
void Base::XMLReader::error(const XERCES_CPP_NAMESPACE_QUALIFIER SAXParseException& e)
{
    // print some details to error output and throw an
    // exception to abort the parsing
    cerr << "Error at file " << StrX(e.getSystemId())
         << ", line " << e.getLineNumber()
         << ", char " << e.getColumnNumber() << endl;
    throw e;
}

void Base::XMLReader::fatalError(const XERCES_CPP_NAMESPACE_QUALIFIER SAXParseException& e)
{
    // print some details to error output and throw an
    // exception to abort the parsing
    cerr << "Fatal Error at file " << StrX(e.getSystemId())
         << ", line " << e.getLineNumber()
         << ", char " << e.getColumnNumber() << endl;
    throw e;
}

void Base::XMLReader::warning(const XERCES_CPP_NAMESPACE_QUALIFIER SAXParseException& e)
{
    // print some details to error output and throw an
    // exception to abort the parsing
   cerr << "Warning at file " << StrX(e.getSystemId())
        << ", line " << e.getLineNumber()
        << ", char " << e.getColumnNumber() << endl;
   throw e;
}

void Base::XMLReader::resetErrors()
{
}
