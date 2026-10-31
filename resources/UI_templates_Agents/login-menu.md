# Login menu (sign-in screen or dialog)
Guideline, not code to copy. Adapt it to the project's framework and keep its structure.

Layout
- One centred card, max width 360-400 px, on a plain or softly tinted background.
- Top to bottom: logo or icon, title ("Sign in"), email field, password field,
  "Forgot password?" link, primary button (full width), a divider, "Create an account" link.
- Fields are stacked, each with a visible label above it (not only a placeholder). Gap: 16 px.

Look
- Card: 12-16 px rounded corners, soft shadow, 24-32 px padding.
- Inputs 40-44 px tall with a 1 px border; the accent colour on focus.
- The primary button uses the accent colour; the secondary links are plain text.
- A user icon and a lock icon inside the fields are welcome (see the icon links given with the task).

Behaviour
- Show/hide password toggle. Enter submits the form. Disable the button while sending.
- Show an error message under the field it belongs to, in red, with text (not colour alone).
- Validate before sending: an email shape and a non-empty password.

Avoid: placeholder-only labels, a wall of social buttons, centred body text, tiny click targets.
