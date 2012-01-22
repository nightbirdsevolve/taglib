/***************************************************************************
    copyright            : (C) 2002 - 2008 by Scott Wheeler
    email                : wheeler@kde.org
 ***************************************************************************/

/***************************************************************************
 *   This library is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU Lesser General Public License version   *
 *   2.1 as published by the Free Software Foundation.                     *
 *                                                                         *
 *   This library is distributed in the hope that it will be useful, but   *
 *   WITHOUT ANY WARRANTY; without even the implied warranty of            *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU     *
 *   Lesser General Public License for more details.                       *
 *                                                                         *
 *   You should have received a copy of the GNU Lesser General Public      *
 *   License along with this library; if not, write to the Free Software   *
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA         *
 *   02110-1301  USA                                                       *
 *                                                                         *
 *   Alternatively, this file is available under the Mozilla Public        *
 *   License Version 1.1.  You may obtain a copy of the License at         *
 *   http://www.mozilla.org/MPL/                                           *
 ***************************************************************************/

#include <tbytevectorlist.h>
#include <id3v2tag.h>

#include "textidentificationframe.h"

using namespace TagLib;
using namespace ID3v2;

class TextIdentificationFrame::TextIdentificationFramePrivate
{
public:
  TextIdentificationFramePrivate() : textEncoding(String::Latin1) {}
  String::Type textEncoding;
  StringList fieldList;
};

////////////////////////////////////////////////////////////////////////////////
// TextIdentificationFrame public members
////////////////////////////////////////////////////////////////////////////////

TextIdentificationFrame::TextIdentificationFrame(const ByteVector &type, String::Type encoding) :
  Frame(type)
{
  d = new TextIdentificationFramePrivate;
  d->textEncoding = encoding;
}

TextIdentificationFrame::TextIdentificationFrame(const ByteVector &data) :
  Frame(data)
{
  d = new TextIdentificationFramePrivate;
  setData(data);
}

TextIdentificationFrame::~TextIdentificationFrame()
{
  delete d;
}

void TextIdentificationFrame::setText(const StringList &l)
{
  d->fieldList = l;
}

void TextIdentificationFrame::setText(const String &s)
{
  d->fieldList = s;
}

String TextIdentificationFrame::toString() const
{
  return d->fieldList.toString();
}

StringList TextIdentificationFrame::fieldList() const
{
  return d->fieldList;
}

String::Type TextIdentificationFrame::textEncoding() const
{
  return d->textEncoding;
}

void TextIdentificationFrame::setTextEncoding(String::Type encoding)
{
  d->textEncoding = encoding;
}

PropertyMap TextIdentificationFrame::asProperties() const
{
  if(frameID() == "TIPL")
    return makeTIPLProperties();
  if(frameID() == "TMCL")
    return makeTMCLProperties();
  PropertyMap map;
  String tagName = frameIDToTagName(frameID());
  if(tagName.isNull()) {
    map.unsupportedData().append(frameID());
    return map;
  }
  StringList values = fieldList();
  if(tagName == "GENRE") {
    // Special case: Support ID3v1-style genre numbers. They are not officially supported in
    // ID3v2, however it seems that still a lot of programs use them.
    for(StringList::Iterator it = values.begin(); it != values.end(); ++it) {
      bool ok = false;
      int test = it->toInt(&ok); // test if the genre value is an integer
      if(ok)
        *it = ID3v1::genre(test);
    }
  } else if(tagName == "DATE") {
    for (StringList::Iterator it = values.begin(); it != values.end(); ++it) {
      // ID3v2 specifies ISO8601 timestamps which contain a 'T' as separator between date and time.
      // Since this is unusual in other formats, the T is removed.
      int tpos = it->find("T");
      if (tpos != -1)
        (*it)[tpos] = ' ';
    }
  }
  return KeyValuePair(tagName, values);
}

////////////////////////////////////////////////////////////////////////////////
// TextIdentificationFrame protected members
////////////////////////////////////////////////////////////////////////////////

void TextIdentificationFrame::parseFields(const ByteVector &data)
{
  // Don't try to parse invalid frames

  if(data.size() < 2)
    return;

  // read the string data type (the first byte of the field data)

  d->textEncoding = String::Type(data[0]);

  // split the byte array into chunks based on the string type (two byte delimiter
  // for unicode encodings)

  int byteAlign = d->textEncoding == String::Latin1 || d->textEncoding == String::UTF8 ? 1 : 2;

  // build a small counter to strip nulls off the end of the field

  int dataLength = data.size() - 1;

  while(dataLength > 0 && data[dataLength] == 0)
    dataLength--;

  while(dataLength % byteAlign != 0)
    dataLength++;

  ByteVectorList l = ByteVectorList::split(data.mid(1, dataLength), textDelimiter(d->textEncoding), byteAlign);

  d->fieldList.clear();

  // append those split values to the list and make sure that the new string's
  // type is the same specified for this frame

  for(ByteVectorList::Iterator it = l.begin(); it != l.end(); it++) {
    if(!(*it).isEmpty()) {
      String s(*it, d->textEncoding);
      d->fieldList.append(s);
    }
  }
}

ByteVector TextIdentificationFrame::renderFields() const
{
  String::Type encoding = checkTextEncoding(d->fieldList, d->textEncoding);

  ByteVector v;

  v.append(char(encoding));

  for(StringList::ConstIterator it = d->fieldList.begin(); it != d->fieldList.end(); it++) {

    // Since the field list is null delimited, if this is not the first
    // element in the list, append the appropriate delimiter for this
    // encoding.

    if(it != d->fieldList.begin())
      v.append(textDelimiter(encoding));

    v.append((*it).data(encoding));
  }

  return v;
}

////////////////////////////////////////////////////////////////////////////////
// TextIdentificationFrame private members
////////////////////////////////////////////////////////////////////////////////

TextIdentificationFrame::TextIdentificationFrame(const ByteVector &data, Header *h) : Frame(h)
{
  d = new TextIdentificationFramePrivate;
  parseFields(fieldData(data));
}

// array of allowed TIPL prefixes and their corresponding key value
static const uint involvedPeopleSize = 5;
static const char* involvedPeople[2] = {
    {"ARRANGER", "ARRANGER"},
    {"ENGINEER", "ENGINEER"},
    {"PRODUCER", "PRODUCER"},
    {"DJ-MIX", "DJMIXER"},
    {"MIX", "MIXER"}
};

PropertyMap TextIdentificationFrame::makeTIPLProperties() const
{
  PropertyMap map;
  if(fieldList().size() % 2 != 0){
    // according to the ID3 spec, TIPL must contain an even number of entries
    map.unsupportedData().append(frameID());
    return map;
  }
  for(StringList::ConstIterator it = fieldList().begin(); it != fieldList().end(); ++it) {
    bool found = false;
    for(uint i = 0; i < involvedPeopleSize; ++i)
      if(*it == involvedPeople[i][0]) {
        map.insert(involvedPeople[i][1], (++it).split(","));
        found = true;
        break;
      }
    if(!found){
      // invalid involved role -> mark whole frame as unsupported in order to be consisten with writing
      map.clear();
      map.unsupportedData().append(frameID());
      return map;
    }
  }
  return map;
}

PropertyMap TextIdentificationFrame::makeTMCLProperties() const
{
  PropertyMap map;
  if(fieldList().size() % 2 != 0){
    // according to the ID3 spec, TMCL must contain an even number of entries
    map.unsupportedData().append(frameID());
    return map;
  }
  for(StringList::ConstIterator it = fieldList().begin(); it != fieldList().end(); ++it) {
    String key = PropertyMap::prepareKey(*it);
    if(key.isNull()) {
      // instrument is not a valid key -> frame unsupported
      map.clear();
      map.unsupportedData().append(frameID());
      return map;
    }
    map.insert(key, (++it).split(","));
  }
  return map;
}

////////////////////////////////////////////////////////////////////////////////
// UserTextIdentificationFrame public members
////////////////////////////////////////////////////////////////////////////////

UserTextIdentificationFrame::UserTextIdentificationFrame(String::Type encoding) :
  TextIdentificationFrame("TXXX", encoding),
  d(0)
{
  StringList l;
  l.append(String::null);
  l.append(String::null);
  setText(l);
}


UserTextIdentificationFrame::UserTextIdentificationFrame(const ByteVector &data) :
  TextIdentificationFrame(data)
{
  checkFields();
}

String UserTextIdentificationFrame::toString() const
{
  return "[" + description() + "] " + fieldList().toString();
}

String UserTextIdentificationFrame::description() const
{
  return !TextIdentificationFrame::fieldList().isEmpty()
    ? TextIdentificationFrame::fieldList().front()
    : String::null;
}

StringList UserTextIdentificationFrame::fieldList() const
{
  // TODO: remove this function

  return TextIdentificationFrame::fieldList();
}

void UserTextIdentificationFrame::setText(const String &text)
{
  if(description().isEmpty())
    setDescription(String::null);

  TextIdentificationFrame::setText(StringList(description()).append(text));
}

void UserTextIdentificationFrame::setText(const StringList &fields)
{
  if(description().isEmpty())
    setDescription(String::null);

  TextIdentificationFrame::setText(StringList(description()).append(fields));
}

void UserTextIdentificationFrame::setDescription(const String &s)
{
  StringList l = fieldList();

  if(l.isEmpty())
    l.append(s);
  else
    l[0] = s;

  TextIdentificationFrame::setText(l);
}

PropertyMap UserTextIdentificationFrame::asProperties() const
{
  String tagName = description();
  // Quodlibet/Exfalso use QuodLibet::<tagname> if you set an arbitrary ID3 tag.
  int pos = tagName.find("::");
  tagName = (pos != -1) ? tagName.substr(pos+2).upper() : tagName.upper();
  PropertyMap map;
  String key = map.prepareKey(tagName);
  if(key.isNull()) // this frame's description is not a valid PropertyMap key -> add to unsupported list
    map.unsupportedData().append(L"TXXX/" + description());
  else
    for(StringList::ConstIterator it = fieldList().begin(); it != fieldList().end(); ++it)
      if(*it != description())
        map.insert(key, *it);
  return map;
}

UserTextIdentificationFrame *UserTextIdentificationFrame::find(
  ID3v2::Tag *tag, const String &description) // static
{
  FrameList l = tag->frameList("TXXX");
  for(FrameList::Iterator it = l.begin(); it != l.end(); ++it) {
    UserTextIdentificationFrame *f = dynamic_cast<UserTextIdentificationFrame *>(*it);
    if(f && f->description() == description)
      return f;
  }
  return 0;
}

////////////////////////////////////////////////////////////////////////////////
// UserTextIdentificationFrame private members
////////////////////////////////////////////////////////////////////////////////

UserTextIdentificationFrame::UserTextIdentificationFrame(const ByteVector &data, Header *h) :
  TextIdentificationFrame(data, h)
{
  checkFields();
}

void UserTextIdentificationFrame::checkFields()
{
  int fields = fieldList().size();

  if(fields == 0)
    setDescription(String::null);
  if(fields <= 1)
    setText(String::null);
}
