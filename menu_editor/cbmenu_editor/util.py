from __future__ import print_function

import gtk
import glib
import gobject

import os
import shlex
import subprocess

from  .icon_browser import IcoBrowse, icobrowse_set_up

class TabButton(gtk.HBox):
	def __init__(self, text):
		gtk.HBox.__init__(self)
		#http://www.eurion.net/python-snippets/snippet/Notebook%20close%20button.html
		self.label = gtk.Label(text)
		self.pack_start(self.label)

		#get a stock close button image
		close_image = gtk.image_new_from_stock(gtk.STOCK_CLOSE, gtk.ICON_SIZE_MENU)
		image_w, image_h = gtk.icon_size_lookup(gtk.ICON_SIZE_MENU)

		#make the close button
		self.btn = gtk.Button()
		self.btn.set_relief(gtk.RELIEF_NONE)
		self.btn.set_focus_on_click(False)
		self.btn.add(close_image)
		self.pack_start(self.btn, False, False)

		#this reduces the size of the button
		style = gtk.RcStyle()
		style.xthickness = 0
		style.ythickness = 0
		self.btn.modify_style(style)

		self.show_all()

class CommandText(gtk.HBox):
	def __init__(self, label_text="Name", mode="Normal", text="", alternate_mode="Execute"):
			gtk.HBox.__init__(self)

			label=gtk.Label(label_text)

			self.entry=gtk.Entry()
			self.entry.props.text=text

			self.button=gtk.Button()
			image=gtk.image_new_from_icon_name("gtk-execute",gtk.ICON_SIZE_LARGE_TOOLBAR)
			self.button.set_image(image)
			#known bug
			self.button.set_tooltip_markup("See the output this command generates")

			self.combobox=gtk.combo_box_new_text()
			self.combobox.append_text("Normal")
			self.combobox.append_text(alternate_mode)
			self.combobox.props.active = mode != "Normal"

			self.pack_start(label,expand=False)
			self.pack_start(self.entry)
			self.pack_end(self.button,expand=False)
			self.pack_end(self.combobox)

			self.combobox.connect('changed', self._emit_mode_signal)
			self.entry.connect('changed', self._emit_text_signal)
			self.button.connect('pressed', self._preview_text)

			self.show_all()

			get_mode=self.combobox.get_active_text()
			if get_mode == "Normal":
				self.button.props.sensitive=0
			else:
				self.button.props.sensitive=1

			if alternate_mode != "Execute" or get_mode != "Normal":
				completion = gtk.EntryCompletion()
				self.entry.set_completion(completion)
				completion.set_model(POSSIBILITY_STORE)
				completion.set_text_column(0)

	def _emit_mode_signal(self, widget):
		text=self.combobox.get_active_text()
		if text == "Normal":
			self.entry.set_completion(None)
			self.button.props.sensitive=0
		else:
			completion = gtk.EntryCompletion()
			self.entry.set_completion(completion)
			completion.set_model(POSSIBILITY_STORE)
			completion.set_text_column(0)
			self.button.props.sensitive=1
		self.emit('mode-changed', text)

	def _emit_text_signal(self, widget):
		self.emit('text-changed', widget.props.text)

	def _preview_text(self, widget):
		print("Generating preview, please wait...")
		text_buffer=gtk.TextBuffer()
		buffer_errors=gtk.TextBuffer()
		full_text=' '.join(['/usr/bin/env',os.path.expanduser(self.entry.props.text)])
		cmd=subprocess.Popen(shlex.split(full_text),stdout=subprocess.PIPE, stderr=subprocess.PIPE)
		text,errors=cmd.communicate()
		if text != None:
			text_buffer.set_text(text)
		else:
			text_buffer.set_text("")

		if errors != None:
			buffer_errors.set_text(errors)
		else:
			buffer_errors.set_text("")

		dialog=gtk.Dialog(title="Preview of %s" %(self.entry.props.text), \
			buttons=(gtk.STOCK_OK, gtk.RESPONSE_ACCEPT))

		tabs=gtk.Notebook()
		tabs.set_scrollable(True)

		scrolled=gtk.ScrolledWindow()
		scrolled.add(gtk.TextView(text_buffer))

		scrolled_errors=gtk.ScrolledWindow()
		scrolled_errors.add(gtk.TextView(buffer_errors))

		tabs.append_page(scrolled, gtk.Label("Output"))
		tabs.append_page(scrolled_errors, gtk.Label("Errors"))

		dialog.vbox.add(tabs)
		dialog.show_all()
		dialog.run()
		dialog.destroy()

gobject.type_register(CommandText)
gobject.signal_new("text-changed", CommandText, gobject.SIGNAL_RUN_FIRST,
                   gobject.TYPE_NONE, (gobject.TYPE_STRING,))
gobject.signal_new("mode-changed", CommandText, gobject.SIGNAL_RUN_FIRST,
                   gobject.TYPE_NONE, (gobject.TYPE_STRING,))

class IconSelector(gtk.HBox):
	def __init__(self, label_text="Icon", mode="Normal", text=""):
			gtk.HBox.__init__(self)

			label=gtk.Label(label_text)
			self.text=text

			self.combobox=gtk.combo_box_new_text()
			self.combobox.append_text("Normal")
			self.combobox.append_text("File path")
			self.combobox.props.active = mode != "Normal"
			self.button=gtk.Button()
			self.image=gtk.Image()

			self.pack_start(label, expand=False)
			self.pack_start(self.button,expand=False)
			self.pack_end(self.combobox, expand=True)
			self.button.set_image(self.image)

			self.combobox.connect('changed', self._emit_mode_signal)
			self.button.connect('pressed', self._emit_text_signal)

			if mode == "File path":
				size=gtk.icon_size_lookup(gtk.ICON_SIZE_LARGE_TOOLBAR)[0]
				try:
					pixbuf=gtk.gdk.pixbuf_new_from_file_at_size(os.path.expanduser(self.text), size, size)
					self.image.set_from_pixbuf(pixbuf)
				except glib.GError:
					self.image.set_from_pixbuf(None)
					print("Couldn't set icon from file: %s" %(self.text))
			else:
				self.image.set_from_icon_name(self.text,gtk.ICON_SIZE_LARGE_TOOLBAR)
			self.button.set_tooltip_text(self.text)

			self.show_all()

	def _change_image(self, mode):
		if mode == "File path":
			size=gtk.icon_size_lookup(gtk.ICON_SIZE_LARGE_TOOLBAR)[0]
			try:
				pixbuf=gtk.gdk.pixbuf_new_from_file_at_size(os.path.expanduser(self.text), size, size)
				self.image.set_from_pixbuf(pixbuf)
			except glib.GError:
				self.image.set_from_pixbuf(None)
				print("Couldn't set icon from file: %s" %(self.text))
		else:
			self.image.set_from_icon_name(self.text,gtk.ICON_SIZE_LARGE_TOOLBAR)
		self.emit('image-changed', mode)

	def _emit_mode_signal(self, widget):
		text=self.combobox.get_active_text()
		self._change_image(text)
		self.emit('mode-changed', text)

	def _emit_text_signal(self, widget):
		text=""
		if self.combobox.get_active_text() == "Normal":
			dialog=IcoBrowse()
			dialog.set_defaults(self.text)
			response = dialog.run()
			if response == gtk.RESPONSE_ACCEPT:
				text=dialog.get_icon_name(None)
			dialog.destroy()
		else:
			btns=(gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.RESPONSE_ACCEPT)
			dialog=gtk.FileChooserDialog(title="Select Icon", buttons=btns)
			dialog.set_filename(text)
			file_filter = gtk.FileFilter()
			file_filter.set_name("Images")
			file_filter.add_mime_type("image/png")
			file_filter.add_mime_type("image/jpeg")
			file_filter.add_mime_type("image/gif")
			file_filter.add_pattern("*.png")
			file_filter.add_pattern("*.jpg")
			file_filter.add_pattern("*.gif")
			file_filter.add_pattern("*.tif")
			file_filter.add_pattern("*.xpm")
			dialog.add_filter(file_filter)
			response = dialog.run()
			if response == gtk.RESPONSE_ACCEPT:
				text=dialog.get_filename()
			dialog.destroy()
		if text != self.text and (text != "" and text != None):
			self.text = text
			self._change_image(self.combobox.get_active_text())
			self.button.set_tooltip_text(self.text)
			self.emit('text-changed', text)

gobject.type_register(IconSelector)
gobject.signal_new("text-changed", IconSelector, gobject.SIGNAL_RUN_FIRST,
                   gobject.TYPE_NONE, (gobject.TYPE_STRING,))
gobject.signal_new("mode-changed", IconSelector, gobject.SIGNAL_RUN_FIRST,
                   gobject.TYPE_NONE, (gobject.TYPE_STRING,))
gobject.signal_new("image-changed", IconSelector, gobject.SIGNAL_RUN_FIRST,
                   gobject.TYPE_NONE, (gobject.TYPE_STRING,))

def completion_setup():
	print("Setting up command auto completion for best experience")
	for i in os.path.expandvars("$PATH").split(":"):
		if os.path.exists(i):
			print("Looking in %s" %i)
			for j in os.listdir(i):
				path="%s/%s" %(i,j)
				if not os.path.isdir(path) and \
				os.access(path, os.X_OK):
					POSSIBILITY_STORE.append([j])

POSSIBILITY_STORE = gtk.ListStore(str)
icobrowse_set_up()
completion_setup()
