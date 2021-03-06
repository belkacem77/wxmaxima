// -*- mode: c++; c-file-style: "linux"; c-basic-offset: 2; indent-tabs-mode: nil -*-
//
//  Copyright (C) 2004-2015 Andrej Vodopivec <andrej.vodopivec@gmail.com>
//            (C) 2014-2016 Gunter Königsmann <wxMaxima@physikbuch.de>
//
//  This program is free software; you can redistribute it and/or modify
//  it under the terms of the GNU General Public License as published by
//  the Free Software Foundation; either version 2 of the License, or
//  (at your option) any later version.
//
//  This program is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//  GNU General Public License for more details.
//
//
//  You should have received a copy of the GNU General Public License
//  along with this program; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
//

#include "TextCell.h"
#include "Setup.h"
#include "wx/config.h"

TextCell::TextCell() : MathCell()
{
  m_text = wxEmptyString;
  m_fontSize = -1;
  m_highlight = false;
  m_altJs = m_alt = false;
}

TextCell::TextCell(wxString text) : MathCell()
{
  m_text = text;
  m_text.Replace(wxT("\n"), wxEmptyString);
  m_highlight = false;
  m_altJs = m_alt = false;
}

TextCell::~TextCell()
{
  if (m_next != NULL)
    delete m_next;
}

void TextCell::SetValue(wxString text)
{
  m_text = text;
  m_width = -1;
  m_text.Replace(wxT("\n"), wxEmptyString);
  m_alt = m_altJs = false;
}

MathCell* TextCell::Copy()
{
  TextCell *retval = new TextCell(wxEmptyString);
  CopyData(this, retval);
  retval->m_text = wxString(m_text);
  retval->m_forceBreakLine = m_forceBreakLine;
  retval->m_bigSkip = m_bigSkip;
  retval->m_isHidden = m_isHidden;
  retval->m_textStyle = m_textStyle;
  retval->m_highlight = m_highlight;

  return retval;
}

void TextCell::Destroy()
{
  m_next = NULL;
}

wxString TextCell::LabelWidthText()
{
  wxString result;

  wxConfig *config = (wxConfig *)wxConfig::Get();
  int labelWidth = 4;
  config->Read(wxT("labelWidth"), &labelWidth);

  for(int i=0;i<labelWidth;i++)
    result += wxT("X");

  return result;
}

void TextCell::RecalculateWidths(CellParser& parser, int fontsize)
{
  SetAltText(parser);

  if (m_height == -1 || m_width == -1 || fontsize != m_fontSize || parser.ForceUpdate())
  {
    m_fontSize = fontsize;

    wxDC& dc = parser.GetDC();
    double scale = parser.GetScale();
    SetFont(parser, fontsize);

    // Labels and prompts are fixed width - adjust font size so that
    // they fit in
    if ((m_textStyle == TS_LABEL) || (m_textStyle == TS_USERLABEL) || (m_textStyle == TS_MAIN_PROMPT)) {
	  // Check for output annotations (/R/ for CRE and /T/ for Taylor expressions)
      if (m_text.Right(2) != wxT("/ "))
        dc.GetTextExtent(wxT("(\%o")+LabelWidthText()+wxT(")"), &m_width, &m_height);
      else
        dc.GetTextExtent(wxT("(\%o")+LabelWidthText()+wxT(")/R/"), &m_width, &m_height);
      m_fontSizeLabel = m_fontSize;
      wxASSERT_MSG((m_width>0)||(m_text==wxEmptyString),_("The letter \"X\" is of width zero. Installing http://www.math.union.edu/~dpvc/jsmath/download/jsMath-fonts.html and checking \"Use JSmath fonts\" in the configuration dialogue should fix it."));
      if(m_width < 1) m_width = 10;
      dc.GetTextExtent(m_text, &m_labelWidth, &m_labelHeight);
      wxASSERT_MSG((m_labelWidth>0)||(m_text==wxEmptyString),_("Seems like something is broken with the maths font. Installing http://www.math.union.edu/~dpvc/jsmath/download/jsMath-fonts.html and checking \"Use JSmath fonts\" in the configuration dialogue should fix it."));
      while ((m_labelWidth >= m_width)&&(m_fontSizeLabel > 2)) {
        int fontsize1 = (int) (((double) --m_fontSizeLabel) * scale + 0.5);
        dc.SetFont(wxFont(fontsize1, wxFONTFAMILY_MODERN,
              parser.IsItalic(m_textStyle),
              parser.IsBold(m_textStyle),
              false, //parser.IsUnderlined(m_textStyle),
              parser.GetFontName(m_textStyle),
              parser.GetFontEncoding()));
        dc.GetTextExtent(m_text, &m_labelWidth, &m_labelHeight);
      }
    }

    /// Check if we are using jsMath and have jsMath character
    else if (m_altJs && parser.CheckTeXFonts())
    {
      dc.GetTextExtent(m_altJsText, &m_width, &m_height);

      if (m_texFontname == wxT("jsMath-cmsy10"))
        m_height = m_height / 2;
    }

    /// We are using a special symbol
    else if (m_alt)
    {
      dc.GetTextExtent(m_altText, &m_width, &m_height);
    }

    /// Empty string has height of X
    else if (m_text == wxEmptyString)
    {
      dc.GetTextExtent(wxT("X"), &m_width, &m_height);
      m_width = 0;
    }

    /// This is the default.
    else
      dc.GetTextExtent(m_text, &m_width, &m_height);

    m_width = m_width + 2 * SCALE_PX(MC_TEXT_PADDING, scale);
    m_height = m_height + 2 * SCALE_PX(MC_TEXT_PADDING, scale);

    /// Hidden cells (multiplication * is not displayed)
    if (m_isHidden)
    {
      m_height = 0;
      m_width = m_width / 4;
    }

    m_realCenter = m_center = m_height / 2;
  }
  ResetData();
}

void TextCell::Draw(CellParser& parser, wxPoint point, int fontsize)
{
  MathCell::Draw(parser, point, fontsize);
  double scale = parser.GetScale();
  wxDC& dc = parser.GetDC();

  wxASSERT_MSG((m_currentPoint.x>0)&&(m_currentPoint.y>0),_("bug: Try to draw text without a position"));
  if (m_width == -1 || m_height == -1)
    RecalculateWidths(parser, fontsize);

  if (DrawThisCell(parser, point) && !m_isHidden)
  {
    SetFont(parser, fontsize);
    SetForeground(parser);

    if(InUpdateRegion())
    {
      /// Labels and prompts have special fontsize
      if ((m_textStyle == TS_LABEL) || (m_textStyle == TS_USERLABEL) || (m_textStyle == TS_MAIN_PROMPT))
      {
        SetFont(parser, m_fontSizeLabel);
        dc.DrawText(m_text,
                    point.x + SCALE_PX(MC_TEXT_PADDING, scale),
                    point.y - m_realCenter + (m_height - m_labelHeight)/2);
      }
      
      /// Check if we are using jsMath and have jsMath character
      else if (m_altJs && parser.CheckTeXFonts())
        dc.DrawText(m_altJsText,
                    point.x + SCALE_PX(MC_TEXT_PADDING, scale),
                    point.y - m_realCenter + SCALE_PX(MC_TEXT_PADDING, scale));
      
      /// We are using a special symbol
      else if (m_alt)
        dc.DrawText(m_altText,
                    point.x + SCALE_PX(MC_TEXT_PADDING, scale),
                    point.y - m_realCenter + SCALE_PX(MC_TEXT_PADDING, scale));
      
      /// Change asterisk
      else if (parser.GetChangeAsterisk() &&  m_text == wxT("*"))
        dc.DrawText(wxT("\xB7"),
                    point.x + SCALE_PX(MC_TEXT_PADDING, scale),
                    point.y - m_realCenter + SCALE_PX(MC_TEXT_PADDING, scale));
      
#if wxUSE_UNICODE
      else if (m_text == wxT("#"))
        dc.DrawText(wxT("\x2260"),
                    point.x + SCALE_PX(MC_TEXT_PADDING, scale),
                    point.y - m_realCenter + SCALE_PX(MC_TEXT_PADDING, scale));
#endif
      /// This is the default.
      else
      {
        switch(GetType())
        {
        case MC_TYPE_TEXT:
          // TODO: Add markdown formatting for bold, italic and underlined here.
          dc.DrawText(m_text,
                      point.x + SCALE_PX(MC_TEXT_PADDING, scale),
                      point.y - m_realCenter + SCALE_PX(MC_TEXT_PADDING, scale));
          break;
        case MC_TYPE_INPUT:
          // This cell has already been drawn as an EditorCell => we don't repeat this action here.
          break;
        default:
          dc.DrawText(m_text,
                      point.x + SCALE_PX(MC_TEXT_PADDING, scale),
                      point.y - m_realCenter + SCALE_PX(MC_TEXT_PADDING, scale));
        }
      }
    }
  }
}

void TextCell::SetFont(CellParser& parser, int fontsize)
{
  wxDC& dc = parser.GetDC();
  double scale = parser.GetScale();

  int fontsize1 = (int) (((double)fontsize) * scale + 0.5);

  if ((m_textStyle == TS_TITLE) ||
      (m_textStyle == TS_SECTION) ||
      (m_textStyle == TS_SUBSECTION) ||
      (m_textStyle == TS_SUBSUBSECTION)
    )
  {
    fontsize1 = parser.GetFontSize(m_textStyle);
    fontsize1 = (int) (((double)fontsize1) * scale + 0.5);
  }

  fontsize1 = MAX(fontsize1, 1);

  // Use jsMath
  if (m_altJs && parser.CheckTeXFonts())
  {
    wxFont font(fontsize1, wxFONTFAMILY_MODERN,
                      wxFONTSTYLE_NORMAL,
                      parser.IsBold(m_textStyle),
                      parser.IsUnderlined(m_textStyle),
                         m_texFontname);
    wxASSERT_MSG(font.IsOk(),_("Seems like something is broken with a font. Installing http://www.math.union.edu/~dpvc/jsmath/download/jsMath-fonts.html and checking \"Use JSmath fonts\" in the configuration dialogue should fix it."));
    dc.SetFont(font);
  }

  // We have an alternative symbol
  else if (m_alt)
  {
    wxFont font(fontsize1, wxFONTFAMILY_MODERN,
                      wxFONTSTYLE_NORMAL,
                      parser.IsBold(m_textStyle),
                      false,
                      m_fontname != wxEmptyString ?
                          m_fontname : parser.GetFontName(m_textStyle),
                  parser.GetFontEncoding());
    wxASSERT_MSG(font.IsOk(),_("Seems like something is broken with a font. Installing http://www.math.union.edu/~dpvc/jsmath/download/jsMath-fonts.html and checking \"Use JSmath fonts\" in the configuration dialogue should fix it."));
    dc.SetFont(font);
  }
  // Titles, sections, subsections... - don't underline
  else if ((m_textStyle == TS_TITLE) ||
           (m_textStyle == TS_SECTION) ||
           (m_textStyle == TS_SUBSECTION) ||
           (m_textStyle == TS_SUBSUBSECTION)
    )
  {
    wxFont font(fontsize1, wxFONTFAMILY_MODERN,
                parser.IsItalic(m_textStyle),
                parser.IsBold(m_textStyle),
                false,
                parser.GetFontName(m_textStyle),
                parser.GetFontEncoding());
    wxASSERT_MSG(font.IsOk(),_("Seems like something is broken with a font. Installing http://www.math.union.edu/~dpvc/jsmath/download/jsMath-fonts.html and checking \"Use JSmath fonts\" in the configuration dialogue should fix it."));
    dc.SetFont(font);
  }
  // Default
  else
  {
    wxFont font(fontsize1, wxFONTFAMILY_MODERN,
                parser.IsItalic(m_textStyle),
                parser.IsBold(m_textStyle),
                parser.IsUnderlined(m_textStyle),
                parser.GetFontName(m_textStyle),
                parser.GetFontEncoding());
    wxASSERT_MSG(font.IsOk(),_("Seems like something is broken with a font. Installing http://www.math.union.edu/~dpvc/jsmath/download/jsMath-fonts.html and checking \"Use JSmath fonts\" in the configuration dialogue should fix it."));
    dc.SetFont(font);
  }
  
  // A fallback if the font we selected is no more installed or isn't working at all.
  if(!dc.GetFont().IsOk())
  {
    dc.SetFont(wxFontInfo(10));
  }
}

bool TextCell::IsOperator()
{
  if (wxString(wxT("+*/-")).Find(m_text) >= 0)
    return true;
#if wxUSE_UNICODE
  if (m_text == wxT("\x2212"))
    return true;
#endif
  return false;
}

wxString TextCell::ToString()
{
  wxString text;
  if (m_altCopyText != wxEmptyString)
    text = m_altCopyText;
  else {
    text = m_text;
#if wxUSE_UNICODE
    text.Replace(wxT("\x2212"), wxT("-")); // unicode minus sign
#endif
  }
  switch(m_textStyle)
  {
  case TS_VARIABLE:
  case TS_FUNCTION:
    // The only way for variable or function names to contain quotes and
    // characters that clearly represent operators is that these chars
    // are quoted by a backslash: They cannot be quoted by quotation
    // marks since maxima would'nt allow strings here.
  {
    // TODO: We could escape the - char. But we get false positives, then.
    wxString charsNeedingQuotes("\\'\"()[]{}^+*/&§?:;=#<>$");
    bool isOperator = true;
    for(int i=0;i<m_text.Length();i++)
    {
      if((m_text[i]==wxT(' ')) || (charsNeedingQuotes.find(m_text[i])==wxNOT_FOUND))
      {
        isOperator = false;
        break;
      }
    }

    if(!isOperator)
      for(int i=0;i<charsNeedingQuotes.Length();i++)
        text.Replace(charsNeedingQuotes[i], wxT("\\") + wxString(charsNeedingQuotes[i]));
  }
    break;
  case TS_STRING:
    text = wxT("\"") + text + wxT("\"");
    break;
  }
  
  return text;
}

wxString TextCell::ToTeX()
{
  wxString text = m_text;
  text.Replace(wxT("\\"), wxT("\\ensuremath{\\backslash}"));
  text.Replace(wxT("<"), wxT("\\ensuremath{<}"));
  text.Replace(wxT(">"), wxT("\\ensuremath{>}"));
#if wxUSE_UNICODE
  text.Replace(wxT("\x2212"), wxT("-")); // unicode minus sign
  text.Replace(L"\x00B1",wxT("\\ensuremath{\\pm}"));
  text.Replace(L"\x03B1",wxT("\\ensuremath{\\alpha}"));
  text.Replace(L"\x00B2",wxT("\\ensuremath{^2}"));
  text.Replace(L"\x00B3",wxT("\\ensuremath{^3}"));
  text.Replace(L"\x221A",wxT("\\ensuremath{\\sqrt{}}"));
  text.Replace(L"\x2148",wxT("\\ensuremath{\\mathbbm{i}}"));
  text.Replace(L"\x2147",wxT("\\ensuremath{\\mathbbm{e}}"));
  text.Replace(L"\x210f",wxT("\\ensuremath{\\hbar}"));
  text.Replace(L"\x2203",wxT("\\ensuremath{\\exists}"));
  text.Replace(L"\x2204",wxT("\\ensuremath{\\nexists}"));
  text.Replace(L"\x2208",wxT("\\ensuremath{\\in}"));
  text.Replace(L"\x21D2",wxT("\\ensuremath{\\Longrightarrow}"));
  text.Replace(L"\x221e",wxT("\\ensuremath{\\infty}"));
  text.Replace(L"\x22C0",wxT("\\ensuremath{\\wedge}"));
  text.Replace(L"\x22C1",wxT("\\ensuremath{\\vee}"));
  text.Replace(L"\x22bb",wxT("\\ensuremath{\\oplus}"));
  text.Replace(L"\x22BC",wxT("\\ensuremath{\\overline{\\wedge}}"));
  text.Replace(L"\x22BB",wxT("\\ensuremath{\\overline{\\vee}}"));
  text.Replace(L"\x00AC",wxT("\\ensuremath{\\setminus}"));
  text.Replace(L"\x22C3",wxT("\\ensuremath{\\cup}"));
  text.Replace(L"\x22C2",wxT("\\ensuremath{\\cap}"));
  text.Replace(L"\x2286",wxT("\\ensuremath{\\subseteq}"));
  text.Replace(L"\x2282",wxT("\\ensuremath{\\subset}"));
  text.Replace(L"\x2288",wxT("\\ensuremath{\\not\\subseteq}"));
  text.Replace(L"\x0127",wxT("\\ensuremath{\\hbar}"));
  text.Replace(L"\x0126",wxT("\\ensuremath{\\Hbar}"));
  text.Replace(L"\x2205",wxT("\\ensuremath{\\emptyset}"));
  text.Replace(L"\x00BD",wxT("\\ensuremath{\\frac{1}{2}}"));
  text.Replace(L"\x03B2",wxT("\\ensuremath{\\beta}"));
  text.Replace(L"\x03B3",wxT("\\ensuremath{\\gamma}"));
  text.Replace(L"\x03B4",wxT("\\ensuremath{\\delta}"));
  text.Replace(L"\x03B5",wxT("\\ensuremath{\\epsilon}"));
  text.Replace(L"\x03B6",wxT("\\ensuremath{\\zeta}"));
  text.Replace(L"\x03B7",wxT("\\ensuremath{\\eta}"));
  text.Replace(L"\x03B8",wxT("\\ensuremath{\\theta}"));
  text.Replace(L"\x03B9",wxT("\\ensuremath{\\iota}"));
  text.Replace(L"\x03BA",wxT("\\ensuremath{\\kappa}"));
  text.Replace(L"\x03BB",wxT("\\ensuremath{\\lambda}"));
  text.Replace(L"\x03BC",wxT("\\ensuremath{\\mu}"));
  text.Replace(L"\x03BD",wxT("\\ensuremath{\\nu}"));
  text.Replace(L"\x03BE",wxT("\\ensuremath{\\xi}"));
  text.Replace(L"\x03BF",wxT("\\ensuremath{\\omnicron}"));
  text.Replace(L"\x03C0",wxT("\\ensuremath{\\pi}"));
  text.Replace(L"\x03C1",wxT("\\ensuremath{\\rho}"));
  text.Replace(L"\x03C3",wxT("\\ensuremath{\\sigma}"));
  text.Replace(L"\x03C4",wxT("\\ensuremath{\\tau}"));
  text.Replace(L"\x03C5",wxT("\\ensuremath{\\upsilon}"));
  text.Replace(L"\x03C6",wxT("\\ensuremath{\\phi}"));
  text.Replace(L"\x03C7",wxT("\\ensuremath{\\chi}"));
  text.Replace(L"\x03C8",wxT("\\ensuremath{\\psi}"));
  text.Replace(L"\x03C9",wxT("\\ensuremath{\\omega}"));
  text.Replace(L"\x0391",wxT("\\ensuremath{\\Alpha}"));
  text.Replace(L"\x0392",wxT("\\ensuremath{\\Beta}"));
  text.Replace(L"\x0393",wxT("\\ensuremath{\\Gamma}"));
  text.Replace(L"\x0394",wxT("\\ensuremath{\\Delta}"));
  text.Replace(L"\x0395",wxT("\\ensuremath{\\Epsilon}"));
  text.Replace(L"\x0396",wxT("\\ensuremath{\\Zeta}"));
  text.Replace(L"\x0397",wxT("\\ensuremath{\\Eta}"));
  text.Replace(L"\x0398",wxT("\\ensuremath{\\Theta}"));
  text.Replace(L"\x0399",wxT("\\ensuremath{\\Iota}"));
  text.Replace(L"\x039A",wxT("\\ensuremath{\\Kappa}"));
  text.Replace(L"\x039B",wxT("\\ensuremath{\\Lambda}"));
  text.Replace(L"\x039C",wxT("\\ensuremath{\\Mu}"));
  text.Replace(L"\x039D",wxT("\\ensuremath{\\Nu}"));
  text.Replace(L"\x039E",wxT("\\ensuremath{\\Xi}"));
  text.Replace(L"\x039F",wxT("\\ensuremath{\\Omnicron}"));
  text.Replace(L"\x03A0",wxT("\\ensuremath{\\Pi}"));
  text.Replace(L"\x03A1",wxT("\\ensuremath{\\Rho}"));
  text.Replace(L"\x03A3",wxT("\\ensuremath{\\Sigma}"));
  text.Replace(L"\x03A4",wxT("\\ensuremath{\\Tau}"));
  text.Replace(L"\x03A5",wxT("\\ensuremath{\\Upsilon}"));
  text.Replace(L"\x03A6",wxT("\\ensuremath{\\Phi}"));
  text.Replace(L"\x03A7",wxT("\\ensuremath{\\Chi}"));
  text.Replace(L"\x03A8",wxT("\\ensuremath{\\Psi}"));
  text.Replace(L"\x03A9",wxT("\\ensuremath{\\Omega}"));
  text.Replace(L"\x2202",wxT("\\ensuremath{\\partial}"));
  text.Replace(L"\x222b",wxT("\\ensuremath{\\int}"));
  text.Replace(L"\x2245",wxT("\\ensuremath{\\approx}"));
  text.Replace(L"\x221d",wxT("\\ensuremath{\\propto}"));
  text.Replace(L"\x2260",wxT("\\ensuremath{\\neq}"));
  text.Replace(L"\x2264",wxT("\\ensuremath{\\leq}"));
  text.Replace(L"\x2265",wxT("\\ensuremath{\\geq}"));
  text.Replace(L"\x226A",wxT("\\ensuremath{\\ll}"));
  text.Replace(L"\x226B",wxT("\\ensuremath{\\gg}"));
  text.Replace(L"\x220e",wxT("\\ensuremath{\\blacksquare}"));
  text.Replace(L"\x2263",wxT("\\ensuremath{\\equiv}"));
  text.Replace(L"\x2211",wxT("\\ensuremath{\\sum}"));
  text.Replace(L"\x220F",wxT("\\ensuremath{\\prod}"));
  text.Replace(L"\x2225",wxT("\\ensuremath{\\parallel}"));
  text.Replace(L"\x27C2",wxT("\\ensuremath{\\bot}"));
  text.Replace(wxT("~"),wxT("\\ensuremath{\\sim }"));
  text.Replace(wxT("_"), wxT("\\_ "));
  text.Replace(wxT("$"), wxT("\\$ "));
  text.Replace(wxT("%"), wxT("\\% "));
  text.Replace(wxT("&"), wxT("\\& "));
  text.Replace(wxT("@"), wxT("\\ensuremath{@}"));
  text.Replace(wxT("#"), wxT("\\ensuremath{\\neq}"));
  text.Replace(wxT("\xDCB6"), wxT("~")); // A non-breakable space
  text.Replace(wxT("<"), wxT("\\ensuremath{<}"));
  text.Replace(wxT(">"), wxT("\\ensuremath{>}"));
  text.Replace(wxT("\x219D"), wxT("\\ensuremath{\\leadsto}"));
  text.Replace(wxT("\x2192"), wxT("\\ensuremath{\\rightarrow}"));
  text.Replace(wxT("\x27F6"), wxT("\\ensuremath{\\longrightarrow}"));
#endif


  // m_IsHidden is set for multiplication signs and parenthesis that
  // don't need to be shown
  if (m_isHidden)
  {
    // Normally in TeX the spacing between variable names following each other directly
    // is chosen to show that this is a multiplication.
    // But any use of \mathit{} will change to ordinary text spacing which means we need
    // to add a \, to show that we want to multiply the two long variable names.
    if((text == wxT("*"))||(text == wxT("\xB7")))
    {
      // We have a hidden multiplication sign
      if(
        // This multiplication sign is between 2 cells
        ((m_previous != NULL)&&(m_next != NULL)) &&
        // These cells are two variable names
        ((m_previous->GetStyle() == TS_VARIABLE) && (m_next->GetStyle() == TS_VARIABLE)) &&
        // The variable name prior to this cell has no subscript
        (!(m_previous->ToString().Contains(wxT('_')))) &&
        // we will be using \mathit{} for the TeX outout.
        ((m_next->ToString().Length() > 1) || (m_next->ToString().Length() > 1))
        )
        text=wxT("\\, ");
      else
      text = wxEmptyString;        
    }
    else
      text = wxEmptyString;
  }
  else
  {
    /*
      Normally we want to draw a centered dot in this case. But if we
      are in the denominator of a d/dt or are drawing the "dx" or
      similar of an integral a centered dot looks stupid and will be
      replaced by a short space ("\,") instead. Likewise we don't want
      to begin a parenthesis with a centered dot even if this
      parenthesis does contain a product.
    */
    
    if (m_SuppressMultiplicationDot)
    {
      text.Replace(wxT("*"),wxT("\\, "));
      text.Replace(wxT("\xB7"),wxT("\\, "));
    }
    else
    {
      // If we want to know if the last element was a "d" we first have to
      // look if there actually is a last element.
      if(m_previous)
      {
        if (m_previous->GetStyle() == TS_SPECIAL_CONSTANT && m_previous->ToTeX()==wxT("d"))
        {
          text.Replace(wxT("*"),wxT("\\, "));
          text.Replace(wxT("\xB7"),wxT("\\, "));
        }
        else
        {
          text.Replace(wxT("*"),wxT("\\cdot "));
          text.Replace(wxT("\xB7"),wxT("\\cdot "));
        }
      }
    }
  }
  
  if (GetStyle() == TS_GREEK_CONSTANT)
  {
    if (text == wxT("\\% alpha"))
      return wxT("\\alpha ");
    else if (text == wxT("\\% beta"))
      return wxT("\\beta ");
    else if (text == wxT("\\% gamma"))
      return wxT("\\gamma ");
    else if (text == wxT("\\% delta"))
      return wxT("\\delta ");
    else if (text == wxT("\\% epsilon"))
      return wxT("\\epsilon ");
    else if (text == wxT("\\% zeta"))
      return wxT("\\zeta ");
    else if (text == wxT("\\% eta"))
      return wxT("\\eta ");
    else if (text == wxT("\\% theta"))
      return wxT("\\theta ");
    else if (text == wxT("\\% iota"))
      return wxT("\\iota ");
    else if (text == wxT("\\% kappa"))
      return wxT("\\kappa ");
    else if (text == wxT("\\% lambda"))
      return wxT("\\lambda ");
    else if (text == wxT("\\% mu"))
      return wxT("\\mu ");
    else if (text == wxT("\\% nu"))
      return wxT("\\nu ");
    else if (text == wxT("\\% xi"))
      return wxT("\\xi ");
    else if (text == wxT("\\% omicron"))
      return wxT("\\omicron ");
    else if (text == wxT("\\% pi"))
      return wxT("\\pi ");
    else if (text == wxT("\\% rho"))
      return wxT("\\rho ");
    else if (text == wxT("\\% sigma"))
      return wxT("\\sigma ");
    else if (text == wxT("\\% tau"))
      return wxT("\\tau ");
    else if (text == wxT("\\% upsilon"))
      return wxT("\\upsilon ");
    else if (text == wxT("\\% phi"))
      return wxT("\\phi ");
    else if (text == wxT("\\% chi"))
      return wxT("\\chi ");
    else if (text == wxT("\\% psi"))
      return wxT("\\psi ");
    else if (text == wxT("\\% omega"))
      return wxT("\\omega ");
    else if (text == wxT("\\% Alpha"))
      return wxT("\\Alpha ");
    else if (text == wxT("\\% Beta"))
      return wxT("\\Beta ");
    else if (text == wxT("\\% Gamma"))
      return wxT("\\Gamma ");
    else if (text == wxT("\\% Delta"))
      return wxT("\\Delta ");
    else if (text == wxT("\\% Epsilon"))
      return wxT("\\Epsilon ");
    else if (text == wxT("\\% Zeta"))
      return wxT("\\Zeta ");
    else if (text == wxT("\\% Eta"))
      return wxT("\\Eta ");
    else if (text == wxT("\\% Theta"))
      return wxT("\\Theta ");
    else if (text == wxT("\\% Iota"))
      return wxT("\\Iota ");
    else if (text == wxT("\\% Kappa"))
      return wxT("\\Kappa ");
    else if (text == wxT("\\% Lambda"))
      return wxT("\\Lambda ");
    else if (text == wxT("\\% Mu"))
      return wxT("\\Mu ");
    else if (text == wxT("\\% Nu"))
      return wxT("\\Nu ");
    else if (text == wxT("\\% Xi"))
      return wxT("\\Xi ");
    else if (text == wxT("\\% Omicron"))
      return wxT("\\Omicron ");
    else if (text == wxT("\\% Pi"))
      return wxT("\\Pi ");
    else if (text == wxT("\\% Rho"))
      return wxT("\\Rho ");
    else if (text == wxT("\\% Sigma"))
      return wxT("\\Sigma ");
    else if (text == wxT("\\% Tau"))
      return wxT("\\Tau ");
    else if (text == wxT("\\% Upsilon"))
      return wxT("\\Upsilon ");
    else if (text == wxT("\\% Phi"))
      return wxT("\\Phi ");
    else if (text == wxT("\\% Chi"))
      return wxT("\\Chi ");
    else if (text == wxT("\\% Psi"))
      return wxT("\\Psi ");
    else if (text == wxT("\\% Omega"))
      return wxT("\\Omega ");
    
    return text;
  }
  
  if (GetStyle() == TS_SPECIAL_CONSTANT)
  {
    if (text == wxT("inf"))
      return wxT("\\infty ");
    else if (text == wxT("%e"))
      return wxT("e");
    else if (text == wxT("%i"))
      return wxT("i");
    else if (text == wxT("\\% pi"))
      return wxT("\\ensuremath{\\pi} ");
    else
      return text;
  }
  
  if ((GetStyle() == TS_LABEL)||(GetStyle() == TS_USERLABEL))
  {
    wxString conditionalLinebreak;
    if(m_previous) conditionalLinebreak = wxT("\\]\n\\[");
    text.Trim(true);
    wxString label = text.SubString(1,text.Length()-2);
    text = conditionalLinebreak + wxT("\\tag{") + label + wxT("}");
    label.Replace(wxT("\\% "),wxT(""));
    text += wxT("\\label{") + label + wxT("}");
  }
  else
  {
    if (GetStyle() == TS_FUNCTION)
    {
      if(text!=wxEmptyString)
        text = wxT("\\operatorname{") + text + wxT("}");
    }
    else if (GetStyle() == TS_VARIABLE)
    {
      if((text.Length() > 1)&&(text[1]!=wxT('_')))
        text = wxT("\\mathit{") + text + wxT("}");
      if (text == wxT("\\% pi"))
        text = wxT("\\ensuremath{\\pi} ");
    }
    else if (GetStyle() == TS_ERROR)
    {
      if(text.Length() > 1)
        text = wxT("\\mbox{") + text + wxT("}");
    }
    else if (GetStyle() == TS_DEFAULT)
    {
      if(text.Length() > 2)
        text = wxT("\\mbox{") + text + wxT("}");
    }
  }
  
  if (
    (GetStyle() != TS_FUNCTION) &&
    (GetStyle() != TS_OUTDATED) &&
    (GetStyle() != TS_VARIABLE) &&
    (GetStyle() != TS_NUMBER)   &&
    (GetStyle() != TS_GREEK_CONSTANT)   &&
    (GetStyle() != TS_SPECIAL_CONSTANT)
    )
    text.Replace(wxT("^"), wxT("\\textasciicircum"));
  
  if((GetStyle() == TS_DEFAULT)||(GetStyle() == TS_STRING))
  {
    if(text.Length()>1)
    {
      if(((m_forceBreakLine)||(m_breakLine)))
        //text=wxT("\\ifhmode\\\\fi\n")+text;
        text = wxT("\\mbox{}\\\\")+text;
      if(GetStyle() != TS_DEFAULT)
        text.Replace(wxT(" "), wxT("\\, "));
    }
  }

  if ((GetStyle() == TS_LABEL)||(GetStyle() == TS_LABEL))
    text = text + wxT(" ");

  return text;
}

wxString TextCell::ToMathML()
{
  wxString text=m_text;
  text.Replace(wxT("&"),wxT("&amp;"));
  text.Replace(wxT("<"),wxT("&lt;"));
  text.Replace(wxT(">"),wxT("&gt;"));
  text.Replace(wxT("'"),  wxT("&apos;"));
  text.Replace(wxT("\""), wxT("&quot;"));

  // If we didn't display a multiplication dot we want to do the same in MathML.
  if(m_isHidden)
  {
    text.Replace(wxT("*"),     wxT("&#8290;"));
    text.Replace(wxT("\xB7"),  wxT("&#8290;"));
    if(text != wxT ("&#8290;"))
      text = wxEmptyString;
  }
  text.Replace(wxT("*"),  wxT("\xB7"));
      
  switch(GetStyle())
  {
  case TS_GREEK_CONSTANT:
    text = GetGreekStringUnicode();
  case TS_SPECIAL_CONSTANT:
  {
    // The "d" from d/dt can be written as a special unicode symbol. But firefox doesn't
    // support this currently => Commenting it out.
    // if((GetStyle() == TS_SPECIAL_CONSTANT) && (text == wxT("d")))
    //   text = wxT("&#2146;");
    bool keepPercent = true;
    wxConfig::Get()->Read(wxT("keepPercent"), &keepPercent);
    if (!keepPercent) {
      if (text == wxT("%e"))
        text = wxT("e");
      else if (text == wxT("%i"))
        text = wxT("i");
    }
  }
  case TS_VARIABLE:
  {
    bool keepPercent = true;
    wxConfig::Get()->Read(wxT("keepPercent"), &keepPercent);
      
    if (!keepPercent) {
      if (text == wxT("%pi"))
        text = wxT("\x03C0");
    }
  }
  case TS_FUNCTION:
    if(text == wxT("inf"))
      text = wxT("\x221e");
    return wxT("<mi>")+text+wxT("</mi>\n");
    break;
  case TS_NUMBER:
    return wxT("<mn>")+text+wxT("</mn>\n");
    break;

  case TS_LABEL:
  case TS_USERLABEL:
    return wxT("<mtext>")+text+wxT("</mtext></mtd><mtd>\n");
    break;

  case TS_STRING:
  default:
    if (text.StartsWith(wxT("\"")))
      return wxT("<ms>")+text+wxT("</ms>\n");
    else
      return wxT("<mo>")+text+wxT("</mo>\n");
  }
}

wxString TextCell::ToXML()
{
  wxString tag;
  wxString flags;
  if(m_isHidden)tag=_T("h");
  else
    switch(GetStyle())
    {
    case TS_GREEK_CONSTANT: tag=_T("g");break;
    case TS_SPECIAL_CONSTANT: tag=_T("s");break;
    case TS_VARIABLE: tag=_T("v");break;
    case TS_FUNCTION: tag=_T("fnm");break;
    case TS_NUMBER: tag=_T("n");break;
    case TS_STRING: tag=_T("st");break;
    case TS_LABEL: tag=_T("lbl");break;
    case TS_USERLABEL: tag=_T("lbl");
      flags += wxT(" userdefined=\"yes\"");
      break;
    default: tag=_T("t");
    }

  if((m_forceBreakLine)&&(GetStyle()!=TS_LABEL)&&(GetStyle()!=TS_USERLABEL))
    flags += wxT(" breakline=\"true\"");
    
  if(GetStyle() == TS_ERROR)
    flags += wxT(" type=\"error\"");
    
  wxString xmlstring = m_text;
  // convert it, so that the XML parser doesn't fail
  xmlstring.Replace(wxT("&"),  wxT("&amp;"));
  xmlstring.Replace(wxT("<"),  wxT("&lt;"));
  xmlstring.Replace(wxT(">"),  wxT("&gt;"));
  xmlstring.Replace(wxT("'"),  wxT("&apos;"));
  xmlstring.Replace(wxT("\""), wxT("&quot;"));

  return _T("<") + tag + flags +_T(">") + xmlstring + _T("</") + tag + _T(">");
}

wxString TextCell::GetDiffPart()
{
  return wxT(",") + m_text + wxT(",1");
}

bool TextCell::IsShortNum()
{
  if (m_next != NULL)
    return false;
  else if (m_text.Length() < 4)
    return true;
  return false;
}

void TextCell::SetAltText(CellParser& parser)
{
  m_altJs = m_alt = false;
  if (GetStyle() == TS_DEFAULT)
    return ;

  /// Greek characters are defined in jsMath, Windows and Unicode
  if (GetStyle() == TS_GREEK_CONSTANT)
  {
    m_altJs = true;
    m_altJsText = GetGreekStringTeX();
    m_texFontname = wxT("jsMath-cmmi10");

#if wxUSE_UNICODE
    m_alt = true;
    m_altText = GetGreekStringUnicode();
#elif defined __WXMSW__
    m_alt = true;
    m_altText = GetGreekStringSymbol();
    m_fontname = wxT("Symbol");
#endif
  }

  /// Check for other symbols
  else {
    m_altJsText = GetSymbolTeX();
    if (m_altJsText != wxEmptyString)
    {
      if (m_text == wxT("+") || m_text == wxT("="))
        m_texFontname = wxT("jsMath-cmr10");
      else if (m_text == wxT("%pi"))
        m_texFontname = wxT("jsMath-cmmi10");
      else
        m_texFontname = wxT("jsMath-cmsy10");
      m_altJs = true;
    }
#if wxUSE_UNICODE
    m_altText = GetSymbolUnicode(parser.CheckKeepPercent());
    if (m_altText != wxEmptyString)
      m_alt = true;
#elif defined __WXMSW__
    m_altText = GetSymbolSymbol(parser.CheckKeepPercent());
    if (m_altText != wxEmptyString)
    {
      m_alt = true;
      m_fontname = wxT("Symbol");
    }
#endif
  }
}

#if wxUSE_UNICODE

wxString TextCell::GetGreekStringUnicode()
{
  wxString txt(m_text);

  if (txt == wxT("gamma"))
    return wxT("\x0393");
  else if (txt == wxT("psi"))
    return wxT("\x03A8");

  if (txt[0] != '%')
    txt = wxT("%") + txt;

  if (txt == wxT("%alpha"))
    return wxT("\x03B1");
  else if (txt == wxT("%beta"))
    return wxT("\x03B2");
  else if (txt == wxT("%gamma"))
    return wxT("\x03B3");
  else if (txt == wxT("%delta"))
    return wxT("\x03B4");
  else if (txt == wxT("%epsilon"))
    return wxT("\x03B5");
  else if (txt == wxT("%zeta"))
    return wxT("\x03B6");
  else if (txt == wxT("%eta"))
    return wxT("\x03B7");
  else if (txt == wxT("%theta"))
    return wxT("\x03B8");
  else if (txt == wxT("%iota"))
    return wxT("\x03B9");
  else if (txt == wxT("%kappa"))
    return wxT("\x03BA");
  else if (txt == wxT("%lambda"))
    return wxT("\x03BB");
  else if (txt == wxT("%mu"))
    return wxT("\x03BC");
  else if (txt == wxT("%nu"))
    return wxT("\x03BD");
  else if (txt == wxT("%xi"))
    return wxT("\x03BE");
  else if (txt == wxT("%omicron"))
    return wxT("\x03BF");
  else if (txt == wxT("%pi"))
    return wxT("\x03C0");
  else if (txt == wxT("%rho"))
    return wxT("\x03C1");
  else if (txt == wxT("%sigma"))
    return wxT("\x03C3");
  else if (txt == wxT("%tau"))
    return wxT("\x03C4");
  else if (txt == wxT("%upsilon"))
    return wxT("\x03C5");
  else if (txt == wxT("%phi"))
    return wxT("\x03C6");
  else if (txt == wxT("%chi"))
    return wxT("\x03C7");
  else if (txt == wxT("%psi"))
    return wxT("\x03C8");
  else if (txt == wxT("%omega"))
    return wxT("\x03C9");
  else if (txt == wxT("%Alpha"))
    return wxT("\x0391");
  else if (txt == wxT("%Beta"))
    return wxT("\x0392");
  else if (txt == wxT("%Gamma"))
    return wxT("\x0393");
  else if (txt == wxT("%Delta"))
    return wxT("\x0394");
  else if (txt == wxT("%Epsilon"))
    return wxT("\x0395");
  else if (txt == wxT("%Zeta"))
    return wxT("\x0396");
  else if (txt == wxT("%Eta"))
    return wxT("\x0397");
  else if (txt == wxT("%Theta"))
    return wxT("\x0398");
  else if (txt == wxT("%Iota"))
    return wxT("\x0399");
  else if (txt == wxT("%Kappa"))
    return wxT("\x039A");
  else if (txt == wxT("%Lambda"))
    return wxT("\x039B");
  else if (txt == wxT("%Mu"))
    return wxT("\x039C");
  else if (txt == wxT("%Nu"))
    return wxT("\x039D");
  else if (txt == wxT("%Xi"))
    return wxT("\x039E");
  else if (txt == wxT("%Omicron"))
    return wxT("\x039F");
  else if (txt == wxT("%Pi"))
    return wxT("\x03A0");
  else if (txt == wxT("%Rho"))
    return wxT("\x03A1");
  else if (txt == wxT("%Sigma"))
    return wxT("\x03A3");
  else if (txt == wxT("%Tau"))
    return wxT("\x03A4");
  else if (txt == wxT("%Upsilon"))
    return wxT("\x03A5");
  else if (txt == wxT("%Phi"))
    return wxT("\x03A6");
  else if (txt == wxT("%Chi"))
    return wxT("\x03A7");
  else if (txt == wxT("%Psi"))
    return wxT("\x03A8");
  else if (txt == wxT("%Omega"))
    return wxT("\x03A9");

  return wxEmptyString;
}

wxString TextCell::GetSymbolUnicode(bool keepPercent)
{
  if (m_text == wxT("+"))
    return wxT("+");
  else if (m_text == wxT("="))
    return wxT("=");
  else if (m_text == wxT("inf"))
    return wxT("\x221E");
  else if (m_text == wxT("%pi"))
    return wxT("\x03C0");
  else if (m_text == wxT("<="))
    return wxT("\x2264");
  else if (m_text == wxT(">="))
    return wxT("\x2265");
#ifndef __WXMSW__
  else if (m_text == wxT(" and "))
    return wxT(" \x22C0 ");
  else if (m_text == wxT(" or "))
    return wxT(" \x22C1 ");
  else if (m_text == wxT(" xor "))
    return wxT(" \x22BB ");
  else if (m_text == wxT(" nand "))
    return wxT(" \x22BC ");
  else if (m_text == wxT(" nor "))
    return wxT(" \x22BD ");
  else if (m_text == wxT(" implies "))
    return wxT(" \x21D2 ");
  else if (m_text == wxT(" equiv "))
    return wxT(" \x21D4 ");
  else if (m_text == wxT("not"))
    return wxT("\x00AC");
  else if (m_text == wxT("->"))
    return wxT("\x2192");
  else if (m_text == wxT("-->"))
    return wxT("\x27F6");
#endif
 /*
  else if (GetStyle() == TS_SPECIAL_CONSTANT && m_text == wxT("d"))
    return wxT("\x2202");
  */

  if (!keepPercent) {
    if (m_text == wxT("%e"))
      return wxT("e");
    else if (m_text == wxT("%i"))
      return wxT("i");
    else if (m_text == wxT("%pi"))
      return wxString(wxT("\x03C0"));
  }

  return wxEmptyString;
}

#elif defined __WXMSW__

wxString TextCell::GetGreekStringSymbol()
{
  if (m_text == wxT("gamma"))
    return wxT("\x47");
  else if (m_text == wxT("zeta"))
    return wxT("\x7A");
  else if (m_text == wxT("psi"))
    return wxT("\x59");

  wxString txt(m_text);
  if (txt[0] != '%')
    txt = wxT("%") + txt;

  if (txt == wxT("%alpha"))
    return wxT("\x61");
  else if (txt == wxT("%beta"))
    return wxT("\x62");
  else if (txt == wxT("%gamma"))
    return wxT("\x67");
  else if (txt == wxT("%delta"))
    return wxT("\x64");
  else if (txt == wxT("%epsilon"))
    return wxT("\x65");
  else if (txt == wxT("%zeta"))
    return wxT("\x7A");
  else if (txt == wxT("%eta"))
    return wxT("\x68");
  else if (txt == wxT("%theta"))
    return wxT("\x71");
  else if (txt == wxT("%iota"))
    return wxT("\x69");
  else if (txt == wxT("%kappa"))
    return wxT("\x6B");
  else if (txt == wxT("%lambda"))
    return wxT("\x6C");
  else if (txt == wxT("%mu"))
    return wxT("\x6D");
  else if (txt == wxT("%nu"))
    return wxT("\x6E");
  else if (txt == wxT("%xi"))
    return wxT("\x78");
  else if (txt == wxT("%omicron"))
    return wxT("\x6F");
  else if (txt == wxT("%pi"))
    return wxT("\x70");
  else if (txt == wxT("%rho"))
    return wxT("\x72");
  else if (txt == wxT("%sigma"))
    return wxT("\x73");
  else if (txt == wxT("%tau"))
    return wxT("\x74");
  else if (txt == wxT("%upsilon"))
    return wxT("\x75");
  else if (txt == wxT("%phi"))
    return wxT("\x66");
  else if (txt == wxT("%chi"))
    return wxT("\x63");
  else if (txt == wxT("%psi"))
    return wxT("\x79");
  else if (txt == wxT("%omega"))
    return wxT("\x77");
  else if (txt == wxT("%Alpha"))
    return wxT("\x41");
  else if (txt == wxT("%Beta"))
    return wxT("\x42");
  else if (txt == wxT("%Gamma"))
    return wxT("\x47");
  else if (txt == wxT("%Delta"))
    return wxT("\x44");
  else if (txt == wxT("%Epsilon"))
    return wxT("\x45");
  else if (txt == wxT("%Zeta"))
    return wxT("\x5A");
  else if (txt == wxT("%Eta"))
    return wxT("\x48");
  else if (txt == wxT("%Theta"))
    return wxT("\x51");
  else if (txt == wxT("%Iota"))
    return wxT("\x49");
  else if (txt == wxT("%Kappa"))
    return wxT("\x4B");
  else if (txt == wxT("%Lambda"))
    return wxT("\x4C");
  else if (txt == wxT("%Mu"))
    return wxT("\x4D");
  else if (txt == wxT("%Nu"))
    return wxT("\x4E");
  else if (txt == wxT("%Xi"))
    return wxT("\x58");
  else if (txt == wxT("%Omicron"))
    return wxT("\x4F");
  else if (txt == wxT("%Pi"))
    return wxT("\x50");
  else if (txt == wxT("%Rho"))
    return wxT("\x52");
  else if (txt == wxT("%Sigma"))
    return wxT("\x53");
  else if (txt == wxT("%Tau"))
    return wxT("\x54");
  else if (txt == wxT("%Upsilon"))
    return wxT("\x55");
  else if (txt == wxT("%Phi"))
    return wxT("\x46");
  else if (txt == wxT("%Chi"))
    return wxT("\x43");
  else if (txt == wxT("%Psi"))
    return wxT("\x59");
  else if (txt == wxT("%Omega"))
    return wxT("\x57");

  return wxEmptyString;
}

wxString TextCell::GetSymbolSymbol(bool keepPercent)
{
  if (m_text == wxT("inf"))
    return "\xA5";
  else if (m_text == wxT("%pi"))
    return "\x70";
  else if (m_text == wxT("->"))
    return "\xAE";
  else if (m_text == wxT(">="))
    return "\xB3";
  else if (m_text == wxT("<="))
    return "\xA3";
  else if (m_text == wxT(" and "))
    return "\xD9";
  else if (m_text == wxT(" or "))
    return "\xDA";
  else if (m_text == wxT("not"))
    return "\xD8";
  else if (m_text == wxT(" nand "))
    return "\xAD";
  else if (m_text == wxT(" nor "))
    return "\xAF";
  else if (m_text == wxT(" implies "))
    return "\xDE";
  else if (m_text == wxT(" equiv "))
    return "\xDB";
  else if (m_text == wxT(" xor "))
    return "\xC5";
  else if (m_text == wxT("~>"))
    return "\x219D";

      
  if (!keepPercent) {
    if (m_text == wxT("%e"))
      return wxT("e");
    else if (m_text == wxT("%i"))
      return wxT("i");
  }

  return wxEmptyString;
}

#endif

wxString TextCell::GetGreekStringTeX()
{
  if (m_text == wxT("gamma"))
    return wxT("\xC0");
  else if (m_text == wxT("zeta"))
    return wxT("\xB0");
  else if (m_text == wxT("psi"))
    return wxT("\xC9");

  wxString txt(m_text);
  if (txt[0] != '%')
    txt = wxT("%") + txt;

  if (txt == wxT("%alpha"))
    return wxT("\xCB");
  else if (txt == wxT("%beta"))
    return wxT("\xCC");
  else if (txt == wxT("%gamma"))
    return wxT("\xCD");
  else if (txt == wxT("%delta"))
    return wxT("\xCE");
  else if (txt == wxT("%epsilon"))
    return wxT("\xCF");
  else if (txt == wxT("%zeta"))
    return wxT("\xB0");
  else if (txt == wxT("%eta"))
    return wxT("\xD1");
  else if (txt == wxT("%theta"))
    return wxT("\xD2");
  else if (txt == wxT("%iota"))
    return wxT("\xD3");
  else if (txt == wxT("%kappa"))
    return wxT("\xD4");
  else if (txt == wxT("%lambda"))
    return wxT("\xD5");
  else if (txt == wxT("%mu"))
    return wxT("\xD6");
  else if (txt == wxT("%nu"))
    return wxT("\xB7");
  else if (txt == wxT("%xi"))
    return wxT("\xD8");
  else if (txt == wxT("%omicron"))
    return wxT("o");
  else if (txt == wxT("%pi"))
    return wxT("\xD9");
  else if (txt == wxT("%rho"))
    return wxT("\xDA");
  else if (txt == wxT("%sigma"))
    return wxT("\xDB");
  else if (txt == wxT("%tau"))
    return wxT("\xDC");
  else if (txt == wxT("%upsilon"))
    return wxT("\xB5");
  else if (txt == wxT("%chi"))
    return wxT("\xDF");
  else if (txt == wxT("%psi"))
    return wxT("\xEF");
  else if (txt == wxT("%phi"))
    return wxT("\x27");
  else if (txt == wxT("%omega"))
    return wxT("\x21");
  else if (txt == wxT("%Alpha"))
    return wxT("A");
  else if (txt == wxT("%Beta"))
    return wxT("B");
  else if (txt == wxT("%Gamma"))
    return wxT("\xC0");
  else if (txt == wxT("%Delta"))
    return wxT("\xC1");
  else if (txt == wxT("%Epsilon"))
    return wxT("E");
  else if (txt == wxT("%Zeta"))
    return wxT("Z");
  else if (txt == wxT("%Eta"))
    return wxT("H");
  else if (txt == wxT("%Theta"))
    return wxT("\xC2");
  else if (txt == wxT("%Iota"))
    return wxT("I");
  else if (txt == wxT("%Kappa"))
    return wxT("K");
  else if (txt == wxT("%Lambda"))
    return wxT("\xC3");
  else if (txt == wxT("%Mu"))
    return wxT("M");
  else if (txt == wxT("%Nu"))
    return wxT("N");
  else if (txt == wxT("%Xi"))
    return wxT("\xC4");
  else if (txt == wxT("%Omicron"))
    return wxT("O");
  else if (txt == wxT("%Pi"))
    return wxT("\xC5");
  else if (txt == wxT("%Rho"))
    return wxT("P");
  else if (txt == wxT("%Sigma"))
    return wxT("\xC6");
  else if (txt == wxT("%Tau"))
    return wxT("T");
  else if (txt == wxT("%Upsilon"))
    return wxT("Y");
  else if (txt == wxT("%Phi"))
    return wxT("\xC8");
  else if (txt == wxT("%Chi"))
    return wxT("X");
  else if (txt == wxT("%Psi"))
    return wxT("\xC9");
  else if (txt == wxT("%Omega"))
    return wxT("\xCA");

  return wxEmptyString;
}

wxString TextCell::GetSymbolTeX()
{
  if (m_text == wxT("inf"))
    return wxT("\x31");
  else if (m_text == wxT("+"))
    return wxT("+");
  else if (m_text == wxT("%pi"))
    return wxT("\xD9");
  else if (m_text == wxT("="))
    return wxT("=");
  else if (m_text == wxT("->"))
    return wxT("\x21");
  else if (m_text == wxT(">="))
    return wxT("\xD5");
  else if (m_text == wxT("<="))
    return wxT("\xD4");
/*
  else if (m_text == wxT(" and "))
    return wxT(" \x5E ");
  else if (m_text == wxT(" or "))
    return wxT(" \x5F ");
  else if (m_text == wxT(" nand "))
    return wxT(" \x22 ");
  else if (m_text == wxT(" nor "))
    return wxT(" \x23 ");
  else if (m_text == wxT(" eq "))
    return wxT(" \x2C ");
  else if (m_text == wxT(" implies "))
    return wxT(" \x29 ");
  else if (m_text == wxT("not"))
    return wxT("\x3A");
  else if (m_text == wxT(" xor "))
    return wxT("\xC8");
*/

  return wxEmptyString;
}
