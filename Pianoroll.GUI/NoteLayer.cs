using System;
using System.Globalization;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Media;
using System.Windows.Media.Animation;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.IO;
using System.Collections.Generic;
using System.Diagnostics;

namespace Pianoroll.GUI
{
    class NoteLayer : FrameworkElement
    {
        List<Visual> children = new List<Visual>();
        Editor ped;
        NotePanel insertNotePanel;

        const int BeatsPerPanel = 2;


        public NotePanel InsertNotePanel { get { return insertNotePanel; } }

        public NoteLayer(Editor ped)
        {
            this.ped = ped;
            insertNotePanel = new NotePanel(ped, 0);
            AddVisualChild(insertNotePanel);
            Recreate();
        }

        public void Recreate()
        {
            Stopwatch sw = new Stopwatch();
            sw.Start();

            foreach (Visual v in children)
                RemoveVisualChild(v);

            children.Clear();

            int npanels = (int)Math.Ceiling((double)ped.PatDim.BeatCount / BeatsPerPanel);
            for (int i = 0; i < npanels; i++)
                children.Add(new NotePanel(ped, i * BeatsPerPanel));

            if (ped.pattern != null)
            {
                foreach (var i in ped.pattern.NativePattern.GetAllNotes(0))
                    AddNote(i);
            }

            foreach (Visual v in children)
                AddVisualChild(v);


            sw.Stop();
            ped.cb.WriteDC(string.Format("NoteLayer {0}ms", sw.ElapsedMilliseconds));

        }


        NotePanel GetNotePanel(NoteEvent ne)
        {
            int ip = ne.Time / (BeatsPerPanel * Global.TicksPerBeat);
            System.Diagnostics.Debug.Assert(ip >= 0);
            System.Diagnostics.Debug.Assert(ip < children.Count);
            if (ip < 0 || ip >= children.Count) return null;
            return (NotePanel)children[ip];
        }

        public void AddNote(NoteEvent ne)
        {
            NotePanel np = GetNotePanel(ne);
            np.AddNote(ne, false);
            np.InvalidateMeasure();
        }

        public void SelectNotes(Int32Rect rect, NoteSet notes, bool add)
        {
            // TODO: optimize
            for (int i = 0; i < children.Count; i++)
                (children[i] as NotePanel).SelectNotes(rect, notes, add);
        }

        public void SelectNotes(int t, NoteSet notes, bool add)
        {
            // TODO: optimize
            for (int i = 0; i < children.Count; i++)
                (children[i] as NotePanel).SelectNotes(t, notes, add);
        }

        public void SelectNote(NoteEvent ne, NoteSet notes, bool add)
        {
            for (int i = 0; i < children.Count; i++)
                (children[i] as NotePanel).SelectNote(ne, notes, add);
        }

        public void DeleteNotes(IEnumerable<NoteEvent> notes)
        {
            foreach (NoteEvent ne in notes)
                GetNotePanel(ne).DeleteNote(ne);
        }

        public void SetVelocity(IEnumerable<NoteEvent> notes, int velocity)
        {
            foreach (NoteEvent ne in notes)
                GetNotePanel(ne).SetVelocity(ne, velocity);
        }

        protected override int VisualChildrenCount
        {
            get { return children.Count + 1; }
        }

        protected override Visual GetVisualChild(int index)
        {
            if (index == children.Count)
                return insertNotePanel;

            if (index < 0 || index >= children.Count)
                throw new ArgumentOutOfRangeException();

            return (Visual)children[index];
        }

        protected override Size MeasureOverride(Size availableSize)
        {
            Size childSize = new Size(double.PositiveInfinity, double.PositiveInfinity);

            foreach (UIElement child in children)
                child.Measure(childSize);

            insertNotePanel.Measure(childSize);

            return new Size();
        }

        void ArrangeChild(NotePanel child, double y)
        {
            double x = 0;
            child.Arrange(new Rect(new Point(x, y), child.DesiredSize));
        }

        protected override Size ArrangeOverride(Size finalSize)
        {
            for (int i = 0; i < children.Count; i++)
            {
                double y = i * BeatsPerPanel * ped.PatDim.BeatHeight;
                ArrangeChild((NotePanel)children[i], y);
            }

            ArrangeChild(insertNotePanel, 0);

            return finalSize; 
        }
    }
}
