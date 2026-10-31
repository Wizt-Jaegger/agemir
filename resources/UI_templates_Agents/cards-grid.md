# Cards grid (list of items such as products, entries, posts)
Guideline, not code to copy. Adapt it to the project's framework and keep its structure.

Layout
- Responsive grid: 1 column on phones, 2 on tablets, 3-4 on desktop. Gap: 16-24 px. Page max width 1200 px.
- Each card: image or icon on top (fixed aspect ratio, cropped, never stretched), title, one or two
  lines of secondary text, and one action (button or link) at the bottom.

Look
- 8-12 px rounded corners, 1 px border or soft shadow; on hover lift by 2-4 px (150 ms).
- Title 16-18 px bold, secondary text 14 px muted. Same card height in one row.

Behaviour
- The whole card is clickable when it opens a detail page. Show an empty state ("Nothing here yet")
  with an icon when the list is empty, and a loading placeholder while data arrives.

Avoid: very long titles that break the layout (truncate with an ellipsis), cards of different sizes.
