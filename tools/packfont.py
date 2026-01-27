#!/bin/env python3

import argparse
from PIL import Image


class Layout(object):

    def __init__(self, filename, srcWidth, srcHeight):
        self.srcWidth = srcWidth
        self.srcHeight = srcHeight

        self.locations = {}

        with open(filename, 'rt', encoding='utf-8') as fp:
            lines = fp.readlines()

        for y in range(len(lines)):
            line = lines[y]
            for x in range(len(line)):
                c = line[x]
                if c in "\r\n":
                    continue
                if c in self.locations:
                    raise KeyError(f'duplicate char in layout: "{c}"')

                self.locations[c] = (x * srcWidth, y * srcHeight)

    def make_empty_glyph(self):
        return [0] * self.srcHeight

    def read_row(self, img, startX, y):
        row = 0
        for x in range(self.srcWidth):
            bit = 1 if img.getpixel((x+startX, y)) > 0 else 0
            row = (row << 1) | bit
        return row

    def get_glyph_rows(self, c, img):
        if c not in self.locations:
            return self.make_empty_glyph()

        loc = self.locations[c]
        rows = [self.read_row(img, loc[0], loc[1]+i) for i in range(self.srcHeight)]
        return rows




def emit_prologue(outfile, imgfilename, width, height):
    outfile.write('//\n')
    outfile.write(f'// PicoCalc LCD display font ({width}x{height})\n')
    outfile.write('//\n')
    outfile.write(f'// This file was created using packfont.py by molen, with data from {imgfilename}\n')
    outfile.write('//\n')
    outfile.write('\n')
    outfile.write('#include "font.h"\n')
    outfile.write('\n')
    outfile.write(f'const font_t font_{width}x{height} = {{\n')
    outfile.write(f'    .width = {width},\n')
    outfile.write(f'    .glyphs = {{\n')


def emit_row(row, width):
    if width > 8:
        return (
            emit_row(row >> 8, width - 8) + 
            ' ' +
            emit_row(row & 0xff, 8) )

    s = '0b'
    msk = 1 << (width-1)
    while msk > 0:
        s += '1' if ((msk & row) == msk) else '0'
        msk = msk >> 1
    s += ','
    return s
    

def emit_glyphs(outfile, img, layout):
    for ix in range(0x7f):
        c = chr(ix)
        rows = layout.get_glyph_rows(c, img)

        outfile.write(f"""        // {hex(ix)} "{c if ix >= 0x20 else ''}"\n""")
        for row in rows:
            outfile.write('        ' + emit_row(row, layout.srcWidth) + '\n')



def emit_epilogue(outfile):
    outfile.write('    }\n')
    outfile.write('};\n')


def main():
    parser = argparse.ArgumentParser(
        prog='packfont',
        description='takes a .png image and a layout file and produces a picocalc-friendly font .c file')
    parser.add_argument('imagefile')
    parser.add_argument('layoutfile')
    parser.add_argument('-o', '--output')
    parser.add_argument('-W', '--glyph-width', type=int, required=True, help='the width of each glyph in the input image (and by default in the output too)')
    parser.add_argument('-H', '--glyph-height', type=int, required=True, help='the height of each glyph in the input image (and by default in the output too)')
    #parser.add_argument('-u', '--output-glyph-width', help='the width of each glyph in the output image')
    #parser.add_argument('-v', '--output-glyph-height', help='the height of each glyph in the output image')
    args = parser.parse_args()
    
    infilename = args.imagefile
    outfilename = args.output if args.output else infilename + '.c'

    with Image.open(infilename) as img:
        img.load()
        img = img.getchannel('A')

    layout = Layout(args.layoutfile, args.glyph_width, args.glyph_height)

    with open(outfilename, 'wt', encoding='utf-8') as outfile:
        emit_prologue(outfile, infilename, args.glyph_width, args.glyph_height)
        emit_glyphs(outfile, img, layout)
        emit_epilogue(outfile)


if __name__ == '__main__':
    main()


