# CARDIACAM

This was a project started many years ago in collaboration with a medical physics lab at Ryerson University (now Toronto Metropolitan University).  This is as far as the work got.  I've played with it a bit since, but for sure it's very bit-rotted now.  Just putting it up here in case it's fun for somebody.

Most of this is gstreamer code.

  * There's a gstreamer element to extract pulse information from a Lightstone 3-finger sensor device made by Wild Divine, and resample it into a uniformly sampled time series.
  * There's a gstreamer bin element to combine opencv bits into a tool to find a face in a video stream and extract the RGB colour components of various parts of the face, presenting them as a time series stream.
  * Some other bits to do an ICA transform on the RGB streams and extract the heart beat.

The best camera we had found at the time for doing this was the Sony PlayStation Eye camera.  The one with the four-element microphone array and standard USB cable.  They were crazy cheap even at the time, and today (~2025) they can be had for just a few dollars from junk bins at second hand stores (at least around Tokyo).  They can capture video at a frame rate of well over 100 Hz, which allows the blood pulse wave to be seen moving across the face, potentially allowing blood pressure and other parameters to be measured in addition to heart rate.
