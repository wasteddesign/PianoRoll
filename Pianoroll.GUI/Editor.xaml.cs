using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;
using System.IO;
using System.Windows;
using System.Windows.Controls;
using System.Windows.Data;
using System.Windows.Documents;
using System.Windows.Input;
using System.Windows.Media;
using System.Windows.Media.Imaging;
using System.Windows.Navigation;
using System.Windows.Shapes;
using System.Windows.Threading;
using System.Windows.Interop;
using System.Runtime.InteropServices;
using Microsoft.Win32;
using System.Reflection;
using BuzzGUI.Common;

namespace Pianoroll.GUI
{
    /// <summary>
    /// Interaction logic for UserControl1.xaml
    /// </summary>
    public partial class Editor : UserControl
    {
        internal Pattern pattern;
        internal IGUICallbacks cb;
        internal PatternDimensions PatDim = new PatternDimensions();
        internal PianoKeyboard pianoKeyboard;
        internal BeatNumberPanel beatNumberPanel;
        internal int lastVelocity = 100;

        Rectangle cursorRectangle;
        Canvas overlayCanvas;

        public BindableData data = new BindableData();

        internal int GridValue { get { return gridBox.SelectedIndex; } }


        public Pattern Pattern
        {
            set
            {
                if (value == pattern)
                    return;

                if (pattern != null)
                {
                    pattern.LengthChangedEvent -= new Pattern.LengthChanged(pattern_LengthChangedEvent);
                }

                pattern = value;
                pianoroll.Pattern = value;

                if (pattern != null)
                {
                    pattern.LengthChangedEvent += new Pattern.LengthChanged(pattern_LengthChangedEvent);

                    PatDim.BeatCount = pattern.Length;
                    UpdatePatternDimensions();

                    pianoroll.Visibility = Visibility.Visible;
                    beatNumberPanel.Visibility = Visibility.Visible;
                    pianoKeyboard.Visibility = Visibility.Visible;
                    this.IsEnabled = true;
                }
                else
                {
                    pianoroll.Visibility = Visibility.Collapsed;
                    beatNumberPanel.Visibility = Visibility.Collapsed;
                    pianoKeyboard.Visibility = Visibility.Collapsed;
                    this.IsEnabled = false;
                }
            }
        }

        void pattern_LengthChangedEvent(Pattern p)
        {
            PatDim.BeatCount = pattern.Length;
            UpdatePatternDimensions();
        }

        public void WindowDeactivated()
        {
        }

        public void KillFocus()
        {
            pianoroll.KillFocus();
        }
   
        public Editor()
        {
//            this.Resources.MergedDictionaries.Add(SharedDictionaryManager.SharedDictionary);
            InitializeComponent();
        }

		public static ResourceDictionary Theme { get; private set; }

		void LoadTheme()
		{
			var path = System.IO.Path.Combine(cb.GetThemePath(), "Gear\\Jeskola Pianoroll\\PianorollResources.xaml");
			if (!File.Exists(path))
			{
				// if file is missing in the theme, use default theme instead
				path = System.IO.Path.Combine(System.IO.Path.GetDirectoryName(cb.GetThemePath()), "Default\\Gear\\Jeskola Pianoroll\\PianorollResources.xaml");
				if (!File.Exists(path))
				{
					MessageBox.Show("\"" + path + "\" is missing", "Jeskola Pianoroll", MessageBoxButton.OK, MessageBoxImage.Error);
					return;
				}
			}

			Theme = XamlReaderEx.LoadHack(path) as ResourceDictionary;
		}

		public void ThemeChanged()
		{
			LoadTheme();

			this.Resources = Theme;
			//Resources.MergedDictionaries.Clear();
			//Resources.MergedDictionaries.Add(Theme);
			pianoroll.ThemeChanged();
			pianoKeyboard.ThemeChanged();
			beatNumberPanel.ThemeChanged();
		}

        public Editor(IGUICallbacks cb)
        {
            System.Windows.Threading.Dispatcher.CurrentDispatcher.UnhandledException += (sender, e) =>
            {
                MessageBox.Show(e.Exception.Message, "Pianoroll");
                e.Handled = true;
            };


            {
//                Uri uri = new Uri("PresentationFramework.Aero;V3.0.0.0;31bf3856ad364e35;component\\themes/aero.normalcolor.xaml", UriKind.Relative);
//                                Uri uri = new Uri("PresentationFramework.Classic;V3.0.0.0;31bf3856ad364e35;component\\themes/classic.xaml", UriKind.Relative);
  //              Uri uri = new Uri("PresentationFramework.Luna;V3.0.0.0;31bf3856ad364e35;component\\themes/luna.normalcolor.xaml", UriKind.Relative);
  //              this.Resources.MergedDictionaries.Add(Application.LoadComponent(uri) as ResourceDictionary);


            }


            //            this.Resources.MergedDictionaries.Add(SharedDictionaryManager.SharedDictionary);
            this.cb = cb;

			LoadTheme();
			Resources.MergedDictionaries.Clear();
			Resources.MergedDictionaries.Add(Theme);

			InitializeComponent();

            this.Loaded += new RoutedEventHandler(Editor_Loaded);
            this.Unloaded += new RoutedEventHandler(Editor_Unloaded);
            this.LayoutUpdated += new EventHandler(Editor_LayoutUpdated);
            this.KeyDown += new KeyEventHandler(Editor_KeyDown);
            this.PreviewKeyDown += new KeyEventHandler(Editor_PreviewKeyDown);
            this.PreviewKeyUp += new KeyEventHandler(Editor_PreviewKeyUp);
            this.PreviewMouseWheel += new MouseWheelEventHandler(Editor_PreviewMouseWheel);
            this.GotFocus += new RoutedEventHandler(Editor_GotFocus);
            
            DataContext = this;

            PatDim.NoteWidth = 16;

            pianoKeyboard = new PianoKeyboard(this);
            Grid.SetRow(pianoKeyboard, 1);
            Grid.SetColumn(pianoKeyboard, 1);
            pianoKeyboard.KeyDown += new PianoKeyboard.PianoKeyDelegate(pianoKeyboard_KeyDown);
            pianoKeyboard.KeyUp += new PianoKeyboard.PianoKeyDelegate(pianoKeyboard_KeyUp);
            grid.Children.Add(pianoKeyboard);

            beatNumberPanel = new BeatNumberPanel(this);
            Grid.SetRow(beatNumberPanel, 2);
            grid.Children.Add(beatNumberPanel);

            overlayCanvas = new Canvas();
            Grid.SetRow(overlayCanvas, 2);
            grid.Children.Add(overlayCanvas);

            pianoroll.editor = this;
            pianoroll.scrollViewer.ScrollChanged += new ScrollChangedEventHandler(pianoroll_ScrollChanged);
            pianoroll.UpdatePatternDimensions();

            cursorRectangle = new Rectangle()
            {
                Style = (Style)FindResource("CursorStyle"),
                IsHitTestVisible = false,
                HorizontalAlignment = HorizontalAlignment.Left,
                DataContext = data
            };

            overlayCanvas.Children.Add(cursorRectangle);

            SetBaseOctave(cb.GetBaseOctave());

            TargetMachineChanged();

            //CompositionTarget.Rendering += new EventHandler(CompositionTarget_Rendering);


			this.IsVisibleChanged += (sender, e) =>
			{
				if (IsVisible && timer == null)
				{
					timer = new DispatcherTimer();
					timer.Interval = TimeSpan.FromMilliseconds(20);
					timer.Tick += CompositionTarget_Rendering;
					timer.Start();
				}
				else if (!IsVisible && timer != null)
				{
					timer.Tick -= CompositionTarget_Rendering;
					timer.Stop();
					timer = null;
				}
			};

		}

		DispatcherTimer timer;

        void Editor_GotFocus(object sender, RoutedEventArgs e)
        {
        }


        public void MachineDestructor()
        {
            //CompositionTarget.Rendering -= new EventHandler(CompositionTarget_Rendering);

        }

        void CompositionTarget_Rendering(object sender, EventArgs e)
        {
            data.SetStateFlags(cb.GetStateFlags());

            int pp = cb.GetPlayPosition();
            if (pp >= 0)
            {
                if (cb.IsEditorWindowVisible())
                {
					int sp = pp * PatDim.BeatHeight / 960;

                    if (sp != pianoroll.scrollViewer.VerticalOffset)
                        pianoroll.scrollViewer.ScrollToVerticalOffset(sp);
                }
                else
                {
                    //cb.WriteDC("Pianoroll editor not visible");
                }
            }

            cb.GetGUINotes();
            cb.GetRecordedNotes();
        }

        void UpdatePatternDimensions()
        {
            pianoroll.UpdatePatternDimensions();
            beatNumberPanel.UpdatePatternDimensions();
            pianoKeyboard.UpdatePatternDimensions();
        }

		public void Update()
		{
			UpdatePatternDimensions();
		}

        int mouseWheelAcc = 0;

        void Editor_PreviewMouseWheel(object sender, MouseWheelEventArgs e)
        {
			if (data.Playing) return;

            pianoroll.Focus();

            mouseWheelAcc += e.Delta;
            
            while (mouseWheelAcc <= -120)
            {
                mouseWheelAcc += 120;
                pianoroll.CursorDown();
            }

            while (mouseWheelAcc >= 120)
            {
                mouseWheelAcc -= 120;
                pianoroll.CursorUp();
            }

            e.Handled = true;
        }

        void Editor_KeyDown(object sender, KeyEventArgs e)
        {
        }

        /*
        static readonly Key[] pianoKeys = 
        {
            Key.Z, Key.S, Key.X, Key.D, Key.C, Key.V, Key.G, Key.B, Key.H, Key.N, Key.J, Key.M,
            Key.Q, Key.D2, Key.W, Key.D3, Key.E, Key.R, Key.D5, Key.T, Key.D6, Key.Y, Key.D7, Key.U,
            Key.I, Key.D9, Key.O, Key.D0, Key.P 
        };
        */


        void Editor_PreviewKeyUp(object sender, KeyEventArgs e)
        {
            if (e.KeyboardDevice.Modifiers == ModifierKeys.None)
            {
                int i = PianoKeyIndex(e);

                if (i != -1)
                {
                    int k = PatDim.FirstMidiNote + 12 * cb.GetBaseOctave() + i;

                    if (pianoKeyboard.IsKeyDown(k))
                    {
                        PianoKeyUp(k, true);
                        e.Handled = true;
                    }

                }
            }
        }

        [DllImport("user32.dll")]
        static extern int MapVirtualKey(int uCode, int uMapType);

        static readonly int[] NoteScanCodes = { 44, 31, 45, 32, 46, 47, 34, 48, 35, 49, 36, 50, 16, 3, 17, 4, 18, 19, 6, 20, 7, 21, 8, 22, 23, 10, 24, 11, 25 };

        int PianoKeyIndex(KeyEventArgs e)
        {
            int vkey = KeyInterop.VirtualKeyFromKey(e.Key);
            int scancode = MapVirtualKey(vkey, 0);
            bool isExtended = (bool)typeof(KeyEventArgs).InvokeMember("IsExtendedKey", BindingFlags.GetProperty | BindingFlags.NonPublic | BindingFlags.Instance, null, e, null);
            if (isExtended) scancode = 0;
            return Array.IndexOf<int>(NoteScanCodes, scancode);
        }

        void Editor_PreviewKeyDown(object sender, KeyEventArgs e)
        {
            if (e.KeyboardDevice.Modifiers == ModifierKeys.None)
            {
                int i = PianoKeyIndex(e);

                if (i != -1)
                {
                    int k = PatDim.FirstMidiNote + 12 * cb.GetBaseOctave() + i;
                    if (!pianoKeyboard.IsKeyDown(k))
                    {
                        PianoKeyDown(k, lastVelocity, true);
                        e.Handled = true;
                    }
                }
            }

            if (e.KeyboardDevice.Modifiers == ModifierKeys.Control || e.KeyboardDevice.Modifiers == (ModifierKeys.Control | ModifierKeys.Shift))
            {
                int g = -1;
                switch (e.Key)
                {
                    case Key.D0: g = 0; break;
                    case Key.D1: g = 1; break;
                    case Key.D2: g = 2; break;
                    case Key.D3: g = 3; break;
                    case Key.D4: g = 4; break;
                    case Key.D5: g = 5; break;
                    case Key.D6: g = 6; break;
                    case Key.D7: g = 7; break;
                    case Key.D8: g = 8; break;
                    case Key.D9: g = 9; break;
                }

                if (g >= 0)
                {
                    if ((e.KeyboardDevice.Modifiers & ModifierKeys.Shift) != 0)
                        g += 10;

                    gridBox.SelectedIndex = g;
                    e.Handled = true;
                }

            }

            if (e.KeyboardDevice.Modifiers == (ModifierKeys.Alt | ModifierKeys.Control))
            {
                if (e.Key == Key.Right)
                {
                    if (PatDim.NoteWidth < 24)
                    {
                        PatDim.NoteWidth += 2;
                        UpdatePatternDimensions();
                    }
                    e.Handled = true;
                }
                else if (e.Key == Key.Left)
                {
                    if (PatDim.NoteWidth > 12)
                    {
                        PatDim.NoteWidth -= 2;
                        UpdatePatternDimensions();
                    }
                    e.Handled = true;
                }
                else if (e.Key == Key.Down)
                {
                    if (PatDim.BeatHeight < 384)
                    {
                        PatDim.BeatHeight *= 2;
                        UpdatePatternDimensions();
                    }
                    e.Handled = true;
                }
                else if (e.Key == Key.Up)
                {
                    if (PatDim.BeatHeight > 12)
                    {
                        PatDim.BeatHeight /= 2;
                        UpdatePatternDimensions();
                    }
                    e.Handled = true;
                }
            }
        }

        void Editor_LayoutUpdated(object sender, EventArgs e)
        {
            cursorRectangle.Width = grid.ActualWidth - System.Windows.SystemParameters.VerticalScrollBarWidth - 6;
            Canvas.SetTop(cursorRectangle, 1 + pianoroll.scrollViewer.ViewportHeight / 2 - cursorRectangle.Height / 2);

        }

        void pianoroll_ScrollChanged(object sender, ScrollChangedEventArgs e)
        {
            int ix = (int)pianoroll.scrollViewer.HorizontalOffset;
            if (ix != pianoroll.scrollViewer.HorizontalOffset)
                pianoroll.scrollViewer.ScrollToHorizontalOffset(ix);
            else
                pianoKeyboard.scrollViewer.ScrollToHorizontalOffset(ix);

            int iy = (int)pianoroll.scrollViewer.VerticalOffset;
            if (iy != pianoroll.scrollViewer.VerticalOffset)
                pianoroll.scrollViewer.ScrollToVerticalOffset(iy);
            else
            {
                beatNumberPanel.sv.ScrollToVerticalOffset(iy);

                int beatnum = iy / PatDim.BeatHeight;
                int beatpos = (iy % PatDim.BeatHeight) * Global.TicksPerBeat / PatDim.BeatHeight;

                cb.SetStatusBarText(0, string.Format("{0}:{1}", beatnum, beatpos));
            }

        }


        void Editor_Loaded(object sender, RoutedEventArgs e)
        {
            pianoroll.Focus();
        }

        void Editor_Unloaded(object sender, RoutedEventArgs e)
        {
        }

        void ShowHelp(object sender, RoutedEventArgs e) { ShowHelp(); }
        public void ShowHelp()
        {
            HelpWindow hw = new HelpWindow()
            {
                WindowStartupLocation = WindowStartupLocation.CenterOwner
            };

            new WindowInteropHelper(hw).Owner = ((HwndSource)PresentationSource.FromVisual(this)).Handle;

            hw.ShowDialog();
        }

        public void MidiNote(int note, int velocity, bool fromuser)
        {
            if (fromuser)
            {
                if (velocity > 0)
                    PianoKeyDown(note, velocity, false);
                else
                    PianoKeyUp(note, false);
            }
            else
            {
                if (velocity > 0)
                    pianoKeyboard.PianoKeyDown(note);
                else
                    pianoKeyboard.PianoKeyUp(note);
            }
        }

        public void pianoKeyboard_KeyDown(int key)
        {
            PianoKeyDown(key, lastVelocity, true);
        }

        public void pianoKeyboard_KeyUp(int key)
        {
            PianoKeyUp(key, true);
        }

        void PianoKeyDown(int key, int vel, bool sendmidi)
        {
            if (sendmidi)
                cb.MidiNote(key, vel);

            pianoKeyboard.PianoKeyDown(key);

            if (!data.Playing)
                pianoroll.PianoKeyDown(key, vel);
        }

        void PianoKeyUp(int index, bool sendmidi)
        {
            if (sendmidi)
                cb.MidiNote(index, 0);

            pianoKeyboard.PianoKeyUp(index);

            if (!data.Playing)
                pianoroll.PianoKeyUp(index);
        }

        public void SetBaseOctave(int bo)
        {
            pianoKeyboard.SetBaseOctave(bo);
        }

        public void TargetMachineChanged()
        {
            errorText.Text = "";

            if (!cb.TargetSet())
                errorText.Text = "No target machine";
            else if (!cb.IsMidiNoteImplemented())
                errorText.Text = "Machine does not implement MidiNote";

        }

        public void AddRecordedNote(NoteEvent ne)
        {
            pianoroll.AddRecordedNote(ne);
        }

        void Import(object sender, RoutedEventArgs e)
        {
            OpenFileDialog ofd = new OpenFileDialog();
            ofd.DefaultExt = ".mid";
            ofd.Filter = "MIDI Files (*.mid)|*.mid";
            if (!(bool)ofd.ShowDialog())
                return;

            NoteSet ns = MidiImporter.Import(ofd.FileName);
            if (ns == null)
                return;

            pattern.NativePattern.BeginAction("Import Midi");

            pattern.NativePattern.DeleteNotes(0, null);
            
            Int32Rect r = ns.Bounds;
            pattern.Length = (int)Math.Ceiling((double)(r.Y + r.Height) / Global.TicksPerBeat);
            
            foreach (NoteEvent ne in ns)
                pattern.NativePattern.AddNote(0, ne);

            UpdatePatternDimensions();
        }

        public int GetEditorPatternPosition()
        {
            return (int)(pianoroll.scrollViewer.VerticalOffset * 4 / PatDim.BeatHeight);
        }

    }
}
