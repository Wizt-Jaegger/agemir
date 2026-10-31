# Hamburger menu (collapsed navigation for small screens)
Guideline, not code to copy. Adapt it to the project's framework and keep its structure.

Layout
- A 40-44 px square button at the top-right of the navbar with a three-line icon.
- Opening it reveals a panel: either a drawer sliding in from the right (280-320 px wide, full height)
  or a full-width list under the navbar. A dimmed backdrop covers the rest of the page.
- Inside: the same links as the navbar, one per row, 48 px tall, large text (16 px or more),
  then the primary action at the bottom.

Look
- The icon turns into an "X" while the menu is open. Slide-in animation of 200-250 ms.
- Active link: accent colour and a left bar. Rows are separated by a thin divider.

Behaviour
- Close on: tapping the X, tapping the backdrop, choosing a link, pressing Escape.
- The button has aria-label="Menu" and aria-expanded set to true or false.
- Stop the page behind from scrolling while the menu is open. Focus moves into the panel.
- Show the hamburger only below 768 px wide; above that show the normal navbar.

Avoid: nested menus more than one level deep, tiny links, a menu that cannot be closed by keyboard.
