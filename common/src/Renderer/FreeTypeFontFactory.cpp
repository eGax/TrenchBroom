/*
 Copyright (C) 2010-2014 Kristian Duske
 
 This file is part of TrenchBroom.
 
 TrenchBroom is free software: you can redistribute it and/or modify
 it under the terms of the GNU General Public License as published by
 the Free Software Foundation, either version 3 of the License, or
 (at your option) any later version.
 
 TrenchBroom is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.
 
 You should have received a copy of the GNU General Public License
 along with TrenchBroom. If not, see <http://www.gnu.org/licenses/>.
 */

#include "FreeTypeFontFactory.h"

#include "Exceptions.h"
#include "IO/Path.h"
#include "IO/SystemPaths.h"
#include "Renderer/FontDescriptor.h"
#include "Renderer/FontTexture.h"
#include "Renderer/TextureFont.h"

namespace TrenchBroom {
    namespace Renderer {
        FreeTypeFontFactory::FreeTypeFontFactory() :
        m_library(NULL) {
            FT_Error error = FT_Init_FreeType(&m_library);
            if (error != 0) {
                m_library = NULL;
                
                RenderException e;
                e << "Error initializing FreeType: " << error;
                throw e;
            }
        }
        
        FreeTypeFontFactory::~FreeTypeFontFactory() {
            if (m_library != NULL) {
                FT_Done_FreeType(m_library);
                m_library = NULL;
            }
        }

        TextureFont* FreeTypeFontFactory::doCreateFont(const FontDescriptor& fontDescriptor) {
            
            FT_Face face = loadFont(fontDescriptor);
            TextureFont* font = buildFont(face, fontDescriptor.minChar(), fontDescriptor.charCount());
            FT_Done_Face(face);
            
            return font;
        }

        FT_Face FreeTypeFontFactory::loadFont(const FontDescriptor& fontDescriptor) {
            const IO::Path fontPath = IO::SystemPaths::findFontFile(fontDescriptor.name());
            
            FT_Face face;
            const FT_Error error = FT_New_Face(m_library, fontPath.asString().c_str(), 0, &face);
            if (error != 0) {
                RenderException e;
                e << "Error loading font '" << fontDescriptor.name() << "': " << error;
                throw e;
            }
            
            const FT_UInt fontSize = static_cast<FT_UInt>(fontDescriptor.size());
            FT_Set_Pixel_Sizes(face, 0, fontSize);
            
            return face;
        }

        TextureFont* FreeTypeFontFactory::buildFont(FT_Face face, const unsigned char firstChar, const unsigned char charCount) {
            const Metrics metrics = computeMetrics(face, firstChar, charCount);

            FontTexture* texture = new FontTexture(charCount, metrics.cellSize, metrics.lineHeight);
            FontGlyphBuilder glyphBuilder(metrics.maxAscend, metrics.cellSize, 3, *texture);
            
            FT_GlyphSlot glyph = face->glyph;
            FontGlyph::List glyphs;
            for (unsigned char c = firstChar; c < firstChar + charCount; ++c) {
                FT_Error error = FT_Load_Char(face, static_cast<FT_ULong>(c), FT_LOAD_RENDER);
                if (error != 0)
                    glyphs.push_back(FontGlyph(0, 0, 0, 0, 0));
                else
                    glyphs.push_back(glyphBuilder.createGlyph(static_cast<size_t>(glyph->bitmap_left),  static_cast<size_t>(glyph->bitmap_top),
                                                              static_cast<size_t>(glyph->bitmap.width), static_cast<size_t>(glyph->bitmap.rows),
                                                              static_cast<size_t>(glyph->advance.x >> 6),
                                                              reinterpret_cast<char*>(glyph->bitmap.buffer),
                                                              static_cast<size_t>(glyph->bitmap.pitch)));
            }
            
            return new TextureFont(texture, glyphs, metrics.lineHeight, firstChar, charCount);
        }

        FreeTypeFontFactory::Metrics FreeTypeFontFactory::computeMetrics(FT_Face face, const unsigned char firstChar, const unsigned char charCount) const {
            FT_GlyphSlot glyph = face->glyph;
            
            int maxWidth = 0;
            int maxAscend = 0;
            int maxDescend = 0;
            int lineHeight = 0;
            
            for (unsigned char c = firstChar; c < firstChar + charCount; ++c) {
                FT_Error error = FT_Load_Char(face, static_cast<FT_ULong>(c), FT_LOAD_RENDER);
                if (error != 0)
                    continue;
                
                maxWidth = std::max(maxWidth, glyph->bitmap_left + glyph->bitmap.width);
                maxAscend = std::max(maxAscend, glyph->bitmap_top);
                maxDescend = std::max(maxDescend, glyph->bitmap.rows - glyph->bitmap_top);
                lineHeight = std::max(lineHeight, static_cast<int>(glyph->metrics.height >> 6));
            }
            
            const int cellSize = std::max(maxWidth, maxAscend + maxDescend);
            
            Metrics metrics;
            metrics.cellSize = static_cast<size_t>(cellSize);
            metrics.maxAscend = static_cast<size_t>(maxAscend);
            metrics.lineHeight = static_cast<size_t>(lineHeight);
            return metrics;
        }
    }
}
