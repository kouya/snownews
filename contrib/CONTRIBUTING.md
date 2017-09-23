# A guide to contributing scripts

So, you've found a great way to extend Snownews' functionality, and want to
share it with the world. That's awesome - thank you! To make sure that everyone
can appreciate your work equally, and with minimal fuss, please follow the
following requirements for your submission. If we all do this, everyone will be
a happier Snownewser!

## Include a general description of what your script does ##

People can't use your work if they don't know what it does. A short, simple and
readable description helps immensely.

## Tell us who you are! ##

Sometimes, people may want to ask you questions about your script, notify you of
breakages or bugs, or send congratulations, thanks or money your way. Ensure
that this is possible - include a contact email in the script somewhere as a
comment.

## License your work ##

Please ensure that your script is licensed, even if it's something trivial.
Please stick to [SPDX][1] licenses - there's no reason for any other
kind in this open-source day and age. Indicate your license choice *clearly*, in
a comment.

## State what you need for the script ##

If people are going to be able to use your work, you need to tell them what
they'll need to have available. This isn't just limited to a scripting language
- if you use any kind of libraries/packages/whatever, list those too! If you
 depend on a particular version of something, say this as well.

## Tell people how to use your script from Snownews ##

Not everyone is a Snownews ninja. It helps to have an example of use from
Snownews as part of what you're describing.

## File it appropriately ##

If your script is meant to be a filter script, put it in the ``filters``
subdirectory. If it's a source script, put it in the ``sources`` subdirectory.
Within those, scripts are grouped by their primary language (Perl, Python, sed,
etc). Please keep to this scheme - it makes finding things much easier.

## Use existing scripts as a formatting guide ##

Uniform formatting helps keep things tidy. Use the existing scripts as a guide.
Include *all* the information above - it matters!

[1]: https://spdx.org/licenses/
