#!/usr/bin/env python3
# -*- coding: utf-8 -*-

#
# SPDX-License-Identifier: GPL-3.0
#
# GNU Radio Python Flow Graph
# Title: Not titled yet
# Author: singsong
# GNU Radio version: 3.10.12.0

from PyQt5 import Qt
from gnuradio import qtgui
from PyQt5 import QtCore
from gnuradio import analog
from gnuradio import audio
from gnuradio import blocks
from gnuradio import filter
from gnuradio.filter import firdes
from gnuradio import gr
from gnuradio.fft import window
import sys
import signal
from PyQt5 import Qt
from argparse import ArgumentParser
from gnuradio.eng_arg import eng_float, intx
from gnuradio import eng_notation
from gnuradio import network
from gnuradio.filter import pfb
import sip
import threading



class duplex_gr(gr.top_block, Qt.QWidget):

    def __init__(self):
        gr.top_block.__init__(self, "Not titled yet", catch_exceptions=True)
        Qt.QWidget.__init__(self)
        self.setWindowTitle("Not titled yet")
        qtgui.util.check_set_qss()
        try:
            self.setWindowIcon(Qt.QIcon.fromTheme('gnuradio-grc'))
        except BaseException as exc:
            print(f"Qt GUI: Could not set Icon: {str(exc)}", file=sys.stderr)
        self.top_scroll_layout = Qt.QVBoxLayout()
        self.setLayout(self.top_scroll_layout)
        self.top_scroll = Qt.QScrollArea()
        self.top_scroll.setFrameStyle(Qt.QFrame.NoFrame)
        self.top_scroll_layout.addWidget(self.top_scroll)
        self.top_scroll.setWidgetResizable(True)
        self.top_widget = Qt.QWidget()
        self.top_scroll.setWidget(self.top_widget)
        self.top_layout = Qt.QVBoxLayout(self.top_widget)
        self.top_grid_layout = Qt.QGridLayout()
        self.top_layout.addLayout(self.top_grid_layout)

        self.settings = Qt.QSettings("gnuradio/flowgraphs", "duplex_gr")

        try:
            geometry = self.settings.value("geometry")
            if geometry:
                self.restoreGeometry(geometry)
        except BaseException as exc:
            print(f"Qt GUI: Could not restore geometry: {str(exc)}", file=sys.stderr)
        self.flowgraph_started = threading.Event()

        ##################################################
        # Variables
        ##################################################
        self.values_freq = values_freq = 0
        self.value = value = 0
        self.samp_rate_tx = samp_rate_tx = 64000
        self.samp_rate_rx2 = samp_rate_rx2 = 48e3
        self.samp_rate_rx = samp_rate_rx = 142857
        self.samp_rate_mic = samp_rate_mic = 11e3

        ##################################################
        # Blocks
        ##################################################

        self._value_range = qtgui.Range(-samp_rate_tx//2, samp_rate_tx//2, 10, 0, 200)
        self._value_win = qtgui.RangeWidget(self._value_range, self.set_value, "'value'", "counter_slider", float, QtCore.Qt.Horizontal)
        self.top_layout.addWidget(self._value_win)
        self._values_freq_range = qtgui.Range(-samp_rate_rx//2, samp_rate_rx//2, 1, 0, 200)
        self._values_freq_win = qtgui.RangeWidget(self._values_freq_range, self.set_values_freq, "'values_freq'", "counter_slider", float, QtCore.Qt.Horizontal)
        self.top_layout.addWidget(self._values_freq_win)
        self.qtgui_freq_sink_x_0_0 = qtgui.freq_sink_c(
            1024, #size
            window.WIN_BLACKMAN_hARRIS, #wintype
            0, #fc
            samp_rate_tx, #bw
            "", #name
            1,
            None # parent
        )
        self.qtgui_freq_sink_x_0_0.set_update_time(0.10)
        self.qtgui_freq_sink_x_0_0.set_y_axis((-140), 10)
        self.qtgui_freq_sink_x_0_0.set_y_label('Relative Gain', 'dB')
        self.qtgui_freq_sink_x_0_0.set_trigger_mode(qtgui.TRIG_MODE_FREE, 0.0, 0, "")
        self.qtgui_freq_sink_x_0_0.enable_autoscale(False)
        self.qtgui_freq_sink_x_0_0.enable_grid(False)
        self.qtgui_freq_sink_x_0_0.set_fft_average(1.0)
        self.qtgui_freq_sink_x_0_0.enable_axis_labels(True)
        self.qtgui_freq_sink_x_0_0.enable_control_panel(False)
        self.qtgui_freq_sink_x_0_0.set_fft_window_normalized(False)



        labels = ['', '', '', '', '',
            '', '', '', '', '']
        widths = [1, 1, 1, 1, 1,
            1, 1, 1, 1, 1]
        colors = ["blue", "red", "green", "black", "cyan",
            "magenta", "yellow", "dark red", "dark green", "dark blue"]
        alphas = [1.0, 1.0, 1.0, 1.0, 1.0,
            1.0, 1.0, 1.0, 1.0, 1.0]

        for i in range(1):
            if len(labels[i]) == 0:
                self.qtgui_freq_sink_x_0_0.set_line_label(i, "Data {0}".format(i))
            else:
                self.qtgui_freq_sink_x_0_0.set_line_label(i, labels[i])
            self.qtgui_freq_sink_x_0_0.set_line_width(i, widths[i])
            self.qtgui_freq_sink_x_0_0.set_line_color(i, colors[i])
            self.qtgui_freq_sink_x_0_0.set_line_alpha(i, alphas[i])

        self._qtgui_freq_sink_x_0_0_win = sip.wrapinstance(self.qtgui_freq_sink_x_0_0.qwidget(), Qt.QWidget)
        self.top_layout.addWidget(self._qtgui_freq_sink_x_0_0_win)
        self.qtgui_freq_sink_x_0 = qtgui.freq_sink_c(
            1024, #size
            window.WIN_BLACKMAN_hARRIS, #wintype
            0, #fc
            samp_rate_rx, #bw
            "", #name
            1,
            None # parent
        )
        self.qtgui_freq_sink_x_0.set_update_time(0.10)
        self.qtgui_freq_sink_x_0.set_y_axis((-140), 10)
        self.qtgui_freq_sink_x_0.set_y_label('Relative Gain', 'dB')
        self.qtgui_freq_sink_x_0.set_trigger_mode(qtgui.TRIG_MODE_FREE, 0.0, 0, "")
        self.qtgui_freq_sink_x_0.enable_autoscale(False)
        self.qtgui_freq_sink_x_0.enable_grid(False)
        self.qtgui_freq_sink_x_0.set_fft_average(0.2)
        self.qtgui_freq_sink_x_0.enable_axis_labels(True)
        self.qtgui_freq_sink_x_0.enable_control_panel(False)
        self.qtgui_freq_sink_x_0.set_fft_window_normalized(False)



        labels = ['', '', '', '', '',
            '', '', '', '', '']
        widths = [1, 1, 1, 1, 1,
            1, 1, 1, 1, 1]
        colors = ["blue", "red", "green", "black", "cyan",
            "magenta", "yellow", "dark red", "dark green", "dark blue"]
        alphas = [1.0, 1.0, 1.0, 1.0, 1.0,
            1.0, 1.0, 1.0, 1.0, 1.0]

        for i in range(1):
            if len(labels[i]) == 0:
                self.qtgui_freq_sink_x_0.set_line_label(i, "Data {0}".format(i))
            else:
                self.qtgui_freq_sink_x_0.set_line_label(i, labels[i])
            self.qtgui_freq_sink_x_0.set_line_width(i, widths[i])
            self.qtgui_freq_sink_x_0.set_line_color(i, colors[i])
            self.qtgui_freq_sink_x_0.set_line_alpha(i, alphas[i])

        self._qtgui_freq_sink_x_0_win = sip.wrapinstance(self.qtgui_freq_sink_x_0.qwidget(), Qt.QWidget)
        self.top_layout.addWidget(self._qtgui_freq_sink_x_0_win)
        self.pfb_arb_resampler_xxx_0_0 = pfb.arb_resampler_ccf(
            (samp_rate_tx/samp_rate_mic),
            taps=None,
            flt_size=32,
            atten=100)
        self.pfb_arb_resampler_xxx_0_0.declare_sample_delay(0)
        self.pfb_arb_resampler_xxx_0 = pfb.arb_resampler_ccf(
            (samp_rate_rx2/samp_rate_rx),
            taps=None,
            flt_size=32,
            atten=100)
        self.pfb_arb_resampler_xxx_0.declare_sample_delay(0)
        self.network_tcp_source_0 = network.tcp_source.tcp_source(itemsize=gr.sizeof_gr_complex*1,addr='127.0.0.1',port=2001,server=True)
        self.network_tcp_sink_0 = network.tcp_sink(gr.sizeof_gr_complex, 1, '127.0.0.1', 2002,2)
        self.low_pass_filter_0_0 = filter.fir_filter_ccf(
            1,
            firdes.low_pass(
                1,
                samp_rate_tx,
                3e3,
                200,
                window.WIN_HAMMING,
                6.76))
        self.low_pass_filter_0 = filter.fir_filter_ccf(
            1,
            firdes.low_pass(
                1,
                samp_rate_rx,
                3e3,
                1000,
                window.WIN_HAMMING,
                6.76))
        self.hilbert_fc_0 = filter.hilbert_fc(65, window.WIN_HAMMING, 6.76)
        self.blocks_wavfile_source_0 = blocks.wavfile_source('/Users/singsong/projects/public/stm32-adc-iq/client/hussein_got_im.wav', True)
        self.blocks_multiply_xx_0 = blocks.multiply_vcc(1)
        self.blocks_complex_to_imag_0 = blocks.complex_to_imag(1)
        self.audio_sink_0 = audio.sink(48000, '', True)
        self.analog_sig_source_x_0 = analog.sig_source_c(samp_rate_mic, analog.GR_COS_WAVE, value, 1, 0, 0)


        ##################################################
        # Connections
        ##################################################
        self.connect((self.analog_sig_source_x_0, 0), (self.blocks_multiply_xx_0, 1))
        self.connect((self.blocks_complex_to_imag_0, 0), (self.audio_sink_0, 0))
        self.connect((self.blocks_multiply_xx_0, 0), (self.pfb_arb_resampler_xxx_0_0, 0))
        self.connect((self.blocks_wavfile_source_0, 0), (self.hilbert_fc_0, 0))
        self.connect((self.hilbert_fc_0, 0), (self.blocks_multiply_xx_0, 0))
        self.connect((self.low_pass_filter_0, 0), (self.pfb_arb_resampler_xxx_0, 0))
        self.connect((self.low_pass_filter_0_0, 0), (self.network_tcp_sink_0, 0))
        self.connect((self.low_pass_filter_0_0, 0), (self.qtgui_freq_sink_x_0_0, 0))
        self.connect((self.network_tcp_source_0, 0), (self.low_pass_filter_0, 0))
        self.connect((self.network_tcp_source_0, 0), (self.qtgui_freq_sink_x_0, 0))
        self.connect((self.pfb_arb_resampler_xxx_0, 0), (self.blocks_complex_to_imag_0, 0))
        self.connect((self.pfb_arb_resampler_xxx_0_0, 0), (self.low_pass_filter_0_0, 0))


    def closeEvent(self, event):
        self.settings = Qt.QSettings("gnuradio/flowgraphs", "duplex_gr")
        self.settings.setValue("geometry", self.saveGeometry())
        self.stop()
        self.wait()

        event.accept()

    def get_values_freq(self):
        return self.values_freq

    def set_values_freq(self, values_freq):
        self.values_freq = values_freq

    def get_value(self):
        return self.value

    def set_value(self, value):
        self.value = value
        self.analog_sig_source_x_0.set_frequency(self.value)

    def get_samp_rate_tx(self):
        return self.samp_rate_tx

    def set_samp_rate_tx(self, samp_rate_tx):
        self.samp_rate_tx = samp_rate_tx
        self.low_pass_filter_0_0.set_taps(firdes.low_pass(1, self.samp_rate_tx, 3e3, 200, window.WIN_HAMMING, 6.76))
        self.pfb_arb_resampler_xxx_0_0.set_rate((self.samp_rate_tx/self.samp_rate_mic))
        self.qtgui_freq_sink_x_0_0.set_frequency_range(0, self.samp_rate_tx)

    def get_samp_rate_rx2(self):
        return self.samp_rate_rx2

    def set_samp_rate_rx2(self, samp_rate_rx2):
        self.samp_rate_rx2 = samp_rate_rx2
        self.pfb_arb_resampler_xxx_0.set_rate((self.samp_rate_rx2/self.samp_rate_rx))

    def get_samp_rate_rx(self):
        return self.samp_rate_rx

    def set_samp_rate_rx(self, samp_rate_rx):
        self.samp_rate_rx = samp_rate_rx
        self.low_pass_filter_0.set_taps(firdes.low_pass(1, self.samp_rate_rx, 3e3, 1000, window.WIN_HAMMING, 6.76))
        self.pfb_arb_resampler_xxx_0.set_rate((self.samp_rate_rx2/self.samp_rate_rx))
        self.qtgui_freq_sink_x_0.set_frequency_range(0, self.samp_rate_rx)

    def get_samp_rate_mic(self):
        return self.samp_rate_mic

    def set_samp_rate_mic(self, samp_rate_mic):
        self.samp_rate_mic = samp_rate_mic
        self.analog_sig_source_x_0.set_sampling_freq(self.samp_rate_mic)
        self.pfb_arb_resampler_xxx_0_0.set_rate((self.samp_rate_tx/self.samp_rate_mic))




def main(top_block_cls=duplex_gr, options=None):

    qapp = Qt.QApplication(sys.argv)

    tb = top_block_cls()

    tb.start()
    tb.flowgraph_started.set()

    tb.show()

    def sig_handler(sig=None, frame=None):
        tb.stop()
        tb.wait()

        Qt.QApplication.quit()

    signal.signal(signal.SIGINT, sig_handler)
    signal.signal(signal.SIGTERM, sig_handler)

    timer = Qt.QTimer()
    timer.start(500)
    timer.timeout.connect(lambda: None)

    qapp.exec_()

if __name__ == '__main__':
    main()
