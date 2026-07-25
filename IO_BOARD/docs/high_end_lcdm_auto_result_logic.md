# LCDM High-End Auto Result Logic

This rule applies only to the LCDM high-end tester UI.

1. K2 starts AUTO test.
2. AUTO test scans every OUT/IN port before deciding the final result.
3. Detected connections are merged by electrical connection group.
4. Each displayed record line is led by the IN side, then lists the OUT side:
   `Ixxx[,Iyyy]-Oxxx[,Oyyy];`
5. One electrical connection group is displayed as one line only. Repeated points
   from the same group must not create duplicate lines.
6. After the full scan is complete, the UI switches to the PASS or NG summary
   screen.
7. From the PASS/NG summary, K3/K4 browse the stored AUTO record pages. K3 moves
   backward; K4 moves forward.
8. K3 and K4 button labels must remain visible. AUTO record footer drawing must
   not cover the key area.
9. No unrelated page jumps are allowed during AUTO result browsing.
