
using System;
using System.Collections.Generic;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Input;
using System.Diagnostics;

namespace Pianoroll.GUI
{
    class PatternBackgroundVisual : FrameworkElement
    {
        Editor editor;

        public PatternBackgroundVisual(Editor editor)
        {
            this.SnapsToDevicePixels = true;
            this.editor = editor;
        }

        protected override void OnRender(DrawingContext dc)
        {
            Stopwatch sw = new Stopwatch();
            sw.Start();

            PatternDimensions pd = editor.PatDim;

            Brush wkbr = (Brush)editor.FindResource("PatternBackgroundWhiteKeyBrush");
            Brush bkbr = (Brush)editor.FindResource("PatternBackgroundBlackKeyBrush");
            Brush lbr = (Brush)editor.FindResource("PatternBackgroundLineBrush");
            wkbr.Freeze();
            bkbr.Freeze();
            lbr.Freeze();

            double width = pd.Width;
            double height = pd.Height;

            for (int note = 0; note < pd.NoteCount; note++)
            {
                int n = note % 12;

                Brush br;

                if (n == 1 || n == 3 || n == 6 || n == 8 || n == 10)
                    br = bkbr;
                else
                    br = wkbr;

                Rect r = new Rect(new Point(note * pd.NoteWidth + 1, 0), new Size(pd.NoteWidth, height));
                dc.DrawRectangle(br, null, r);


            }

            for (int beat = 0; beat <= pd.BeatCount; beat++)
            {
                dc.DrawRectangle(lbr, null, new Rect(0, beat * pd.BeatHeight, width, 1.0));
            }

            for (int note = 0; note <= pd.NoteCount; note++)
            {
                dc.DrawRectangle(lbr, null, new Rect(note * pd.NoteWidth, 0, 1.0, height));

            }

            sw.Stop();
            editor.cb.WriteDC(string.Format("PatternBackgroundVisual {0}ms", sw.ElapsedMilliseconds));

        }



    }

}
