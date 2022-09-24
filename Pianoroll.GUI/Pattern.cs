using System;
using System.Collections.Generic;
using System.Linq;
using System.Text;

namespace Pianoroll.GUI
{
    public class Pattern
    {
        INativePattern nativePattern;
        int length;

        public delegate void LengthChanged(Pattern  p);
        public event LengthChanged LengthChangedEvent;

        internal int Length
        {
            get { return length; }
            set
            {
                length = value;
                nativePattern.SetHostLength(length * 4);

                if (LengthChangedEvent != null)
                    LengthChangedEvent(this);
            }

        }

        public int LengthInTicks { get { return length * Global.TicksPerBeat; } }

        public int LengthInBuzzTicks
        {
            set
            {
                length = LengthFromNumBuzzTicks(value);

                if (LengthChangedEvent != null)
                    LengthChangedEvent(this);
            }
        }

        public INativePattern NativePattern { get { return nativePattern; } }

        int LengthFromNumBuzzTicks(int n)
        {
            return n / 4;
        }

        public Pattern(INativePattern np, int numbuzzticks)
        {
            nativePattern = np;
            length = LengthFromNumBuzzTicks(numbuzzticks);
            nativePattern.AddTrack();

        }

		public void Update()
		{
			if (LengthChangedEvent != null)
				LengthChangedEvent(this);

		}


    }
}
