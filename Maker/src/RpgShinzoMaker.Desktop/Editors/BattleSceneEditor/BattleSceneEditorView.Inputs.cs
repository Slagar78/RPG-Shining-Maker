using System;
using System.Linq;
using Avalonia;
using Avalonia.Controls;
using Avalonia.Controls.Primitives;
using Avalonia.Input;
using Avalonia.Interactivity;
using Avalonia.Layout;
using Avalonia.Media;
using Avalonia.Threading;
using RpgShinzoMaker.Core.Models;

namespace RpgShinzoMaker.Desktop.Editors.BattleSceneEditor;

public partial class BattleSceneEditorView
{
    private readonly PhaseControls[,] _phaseControls = new PhaseControls[2, 3];

    private class PhaseControls
    {
        public TextBox TxtX { get; set; } = null!;
        public TextBox TxtY { get; set; } = null!;
        public TextBox TxtDur { get; set; } = null!;
        public TextBlock LblFrame { get; set; } = null!;
        public CheckBox ChkAnim { get; set; } = null!;
    }

    private void InitializeInputs()
    {
        _phaseControls[0, 0] = CreatePhaseRow(0, 0, AllyIdleControls);
        _phaseControls[0, 1] = CreatePhaseRow(0, 1, AllyAttackControls);
        _phaseControls[0, 2] = CreatePhaseRow(0, 2, AllyDodgeControls);
        _phaseControls[1, 0] = CreatePhaseRow(1, 0, EnemyIdleControls);
        _phaseControls[1, 1] = CreatePhaseRow(1, 1, EnemyAttackControls);
        _phaseControls[1, 2] = CreatePhaseRow(1, 2, EnemyDodgeControls);
        UpdateInputsUI();
    }

    private PhaseControls CreatePhaseRow(int unit, int phase, Grid container)
    {
        var pc = new PhaseControls();
        
        container.ColumnDefinitions = new ColumnDefinitions("Auto, Auto, Auto, Auto, Auto, Auto, Auto, Auto, Auto, Auto, Auto, Auto, Auto, Auto, Auto, Auto");
        int col = 0;

        // 1. X
        var lblX = new TextBlock { Text = "X", Foreground = Brushes.LightGray, VerticalAlignment = VerticalAlignment.Center, FontSize = 10, Margin = new Thickness(0,0,2,0) };
        Grid.SetColumn(lblX, col++); container.Children.Add(lblX);

        // УБРАН TextAlignment = TextAlignment.Center
        pc.TxtX = new TextBox { Width = 40, Height = 20, Padding = new Thickness(0), FontSize = 10, VerticalContentAlignment = VerticalAlignment.Center, Margin = new Thickness(0,0,2,0) };
        SetupNumericBox(pc.TxtX);
        pc.TxtX.LostFocus += (s, e) => ApplyTextInput(unit, phase, 0);
        pc.TxtX.KeyDown += (s, e) => { if (e.Key == Key.Enter) ApplyTextInput(unit, phase, 0); };
        Grid.SetColumn(pc.TxtX, col++); container.Children.Add(pc.TxtX);

        var btnXDec = new RepeatButton { Content = "<", Width = 20, Height = 20, Delay = 300, Interval = 100, FontSize = 10, Padding = new Thickness(0), Margin = new Thickness(0, 0, 1, 0) };
        btnXDec.Click += (s, e) => ChangeOffset(unit, phase, 0, -1);
        Grid.SetColumn(btnXDec, col++); container.Children.Add(btnXDec);

        var btnXInc = new RepeatButton { Content = ">", Width = 20, Height = 20, Delay = 300, Interval = 100, FontSize = 10, Padding = new Thickness(0), Margin = new Thickness(0, 0, 10, 0) };
        btnXInc.Click += (s, e) => ChangeOffset(unit, phase, 0, 1);
        Grid.SetColumn(btnXInc, col++); container.Children.Add(btnXInc);

        // 2. Y
        var lblY = new TextBlock { Text = "Y", Foreground = Brushes.LightGray, VerticalAlignment = VerticalAlignment.Center, FontSize = 10, Margin = new Thickness(0,0,2,0) };
        Grid.SetColumn(lblY, col++); container.Children.Add(lblY);

        pc.TxtY = new TextBox { Width = 40, Height = 20, Padding = new Thickness(0), FontSize = 10, VerticalContentAlignment = VerticalAlignment.Center, Margin = new Thickness(0,0,2,0) };
        SetupNumericBox(pc.TxtY);
        pc.TxtY.LostFocus += (s, e) => ApplyTextInput(unit, phase, 1);
        pc.TxtY.KeyDown += (s, e) => { if (e.Key == Key.Enter) ApplyTextInput(unit, phase, 1); };
        Grid.SetColumn(pc.TxtY, col++); container.Children.Add(pc.TxtY);

        var btnYDec = new RepeatButton { Content = "<", Width = 20, Height = 20, Delay = 300, Interval = 100, FontSize = 10, Padding = new Thickness(0), Margin = new Thickness(0, 0, 1, 0) };
        btnYDec.Click += (s, e) => ChangeOffset(unit, phase, 1, -1);
        Grid.SetColumn(btnYDec, col++); container.Children.Add(btnYDec);

        var btnYInc = new RepeatButton { Content = ">", Width = 20, Height = 20, Delay = 300, Interval = 100, FontSize = 10, Padding = new Thickness(0), Margin = new Thickness(0, 0, 10, 0) };
        btnYInc.Click += (s, e) => ChangeOffset(unit, phase, 1, 1);
        Grid.SetColumn(btnYInc, col++); container.Children.Add(btnYInc);

        // 3. Dur
        var lblDur = new TextBlock { Text = "D", Foreground = Brushes.LightGray, VerticalAlignment = VerticalAlignment.Center, FontSize = 10, Margin = new Thickness(0,0,2,0) };
        Grid.SetColumn(lblDur, col++); container.Children.Add(lblDur);

        pc.TxtDur = new TextBox { Width = 40, Height = 20, Padding = new Thickness(0), FontSize = 10, VerticalContentAlignment = VerticalAlignment.Center, Margin = new Thickness(0,0,2,0) };
        pc.TxtDur.LostFocus += (s, e) => ApplyTextInput(unit, phase, 2);
        pc.TxtDur.KeyDown += (s, e) => { if (e.Key == Key.Enter) ApplyTextInput(unit, phase, 2); };
        Grid.SetColumn(pc.TxtDur, col++); container.Children.Add(pc.TxtDur);

        var btnDurDec = new RepeatButton { Content = "-", Width = 20, Height = 20, Delay = 300, Interval = 100, FontSize = 10, Padding = new Thickness(0), Margin = new Thickness(0, 0, 1, 0) };
        btnDurDec.Click += (s, e) => ChangeDuration(unit, phase, -0.01f);
        Grid.SetColumn(btnDurDec, col++); container.Children.Add(btnDurDec);

        var btnDurInc = new RepeatButton { Content = "+", Width = 20, Height = 20, Delay = 300, Interval = 100, FontSize = 10, Padding = new Thickness(0), Margin = new Thickness(0, 0, 10, 0) };
        btnDurInc.Click += (s, e) => ChangeDuration(unit, phase, 0.01f);
        Grid.SetColumn(btnDurInc, col++); container.Children.Add(btnDurInc);

        // 4. Frame
        pc.LblFrame = new TextBlock { Text = "0/0", Foreground = Brushes.LightGray, VerticalAlignment = VerticalAlignment.Center, MinWidth = 30, TextAlignment = TextAlignment.Center, FontSize = 10, Margin = new Thickness(0,0,2,0) };
        Grid.SetColumn(pc.LblFrame, col++); container.Children.Add(pc.LblFrame);

        var btnFrameDec = new RepeatButton { Content = "<", Width = 20, Height = 20, Delay = 300, Interval = 100, FontSize = 10, Padding = new Thickness(0), Margin = new Thickness(0, 0, 1, 0) };
        btnFrameDec.Click += (s, e) => ChangeFrame(unit, phase, -1);
        Grid.SetColumn(btnFrameDec, col++); container.Children.Add(btnFrameDec);

        var btnFrameInc = new RepeatButton { Content = ">", Width = 20, Height = 20, Delay = 300, Interval = 100, FontSize = 10, Padding = new Thickness(0), Margin = new Thickness(0, 0, 10, 0) };
        btnFrameInc.Click += (s, e) => ChangeFrame(unit, phase, 1);
        Grid.SetColumn(btnFrameInc, col++); container.Children.Add(btnFrameInc);

        // 5. Anim Checkbox
        pc.ChkAnim = new CheckBox { Content = "A", Foreground = Brushes.LightGray, FontSize = 10, Padding = new Thickness(0), MinWidth = 0, VerticalAlignment = VerticalAlignment.Center };
        pc.ChkAnim.IsCheckedChanged += (s, e) => ToggleAnim(unit, phase, pc.ChkAnim.IsChecked == true);
        Grid.SetColumn(pc.ChkAnim, col++); container.Children.Add(pc.ChkAnim);

        return pc;
    }

    private void SetupNumericBox(TextBox tb)
    {
        tb.AddHandler(TextInputEvent, OnNumericTextInput, RoutingStrategies.Tunnel);

        // 1. При клике мышкой (фокусе) - палочка в конец
        tb.GotFocus += (s, e) =>
        {
            if (s is TextBox t)
            {
                Dispatcher.UIThread.Post(() =>
                {
                    t.CaretIndex = (t.Text ?? "").Length;
                }, DispatcherPriority.Input);
            }
        };

        // 2. При любом изменении текста (Backspace, Delete, ввод) - палочка в конец
        tb.TextChanged += (s, e) =>
        {
            if (s is TextBox t && t.IsFocused)
                t.CaretIndex = t.Text?.Length ?? 0;
        };
    }

    private void OnNumericTextInput(object? sender, TextInputEventArgs e)
    {
        if (sender is not TextBox tb) return;
        var input = e.Text;
        if (string.IsNullOrEmpty(input)) return;

        if (!input.All(c => char.IsDigit(c) || c == '-'))
        {
            e.Handled = true;
            return;
        }

        if (input.Contains('-'))
        {
            if (tb.CaretIndex != 0 || (tb.Text ?? "").StartsWith("-"))
            {
                e.Handled = true;
                return;
            }
        }

        var currentText = tb.Text ?? "";
        var selectionLength = tb.SelectionEnd - tb.SelectionStart;
        var newLength = currentText.Length - selectionLength + input.Length;
        var hasMinus = currentText.StartsWith("-") || (input == "-" && tb.CaretIndex == 0);
        var maxLength = hasMinus ? 5 : 4;

        if (newLength > maxLength)
        {
            e.Handled = true;
        }
    }

    private AnimationSet GetAnimSet(int unit) => (unit == 0) ? AllyAnim : EnemyAnim;

    private void ApplyTextInput(int unit, int phase, int target)
    {
        var set = GetAnimSet(unit);
        if (!set.Loaded) return;
        var ph = set.GetPhase(phase);
        if (ph.Frames.Count == 0) return;
        int idx = set.CurrentFrame[phase] % ph.Frames.Count;
        var frame = ph.Frames[idx];
        var pc = _phaseControls[unit, phase];
        
        try
        {
            if (target == 0 && int.TryParse(pc.TxtX.Text, out int x)) frame.OffsetX = Math.Clamp(x, -9999, 9999);
            else if (target == 1 && int.TryParse(pc.TxtY.Text, out int y)) frame.OffsetY = Math.Clamp(y, -9999, 9999);
            else if (target == 2 && float.TryParse(pc.TxtDur.Text, out float d)) frame.Duration = Math.Clamp(d, 0.01f, 5.0f);
        }
        catch { }
        UpdateInputsUI();
        UpdatePreview();
    }

    private void ChangeOffset(int unit, int phase, int axis, int direction)
    {
        var set = GetAnimSet(unit);
        if (!set.Loaded) return;
        var ph = set.GetPhase(phase);
        if (ph.Frames.Count == 0) return;
        int idx = set.CurrentFrame[phase] % ph.Frames.Count;
        
        if (axis == 0) ph.Frames[idx].OffsetX = Math.Clamp(ph.Frames[idx].OffsetX + direction, -9999, 9999);
        else ph.Frames[idx].OffsetY = Math.Clamp(ph.Frames[idx].OffsetY + direction, -9999, 9999);

        UpdateInputsUI();
        UpdatePreview();
    }

    private void ChangeDuration(int unit, int phase, float delta)
    {
        var set = GetAnimSet(unit);
        if (!set.Loaded) return;
        var ph = set.GetPhase(phase);
        if (ph.Frames.Count == 0) return;
        int idx = set.CurrentFrame[phase] % ph.Frames.Count;
        ph.Frames[idx].Duration = Math.Clamp(ph.Frames[idx].Duration + delta, 0.01f, 5.0f);
        UpdateInputsUI();
        UpdatePreview();
    }

    private void ChangeFrame(int unit, int phase, int direction)
    {
        var set = GetAnimSet(unit);
        if (!set.Loaded) return;
        var ph = set.GetPhase(phase);
        if (ph.Frames.Count == 0) return;
        set.CurrentFrame[phase] += direction;
        if (set.CurrentFrame[phase] < 0) set.CurrentFrame[phase] = ph.Frames.Count - 1;
        set.CurrentFrame[phase] %= ph.Frames.Count;
        UpdateInputsUI();
        UpdatePreview();
    }

    private void ToggleAnim(int unit, int phase, bool isChecked)
    {
        var set = GetAnimSet(unit);
        if (!set.Loaded) return;
        var ph = set.GetPhase(phase);
        ph.Animate = isChecked;
        if (isChecked)
        {
            for (int p = 0; p < 3; p++) if (p != phase) set.GetPhase(p).Animate = false;
            set.CurrentPhase = phase; ActiveSlot = unit; ActivePhase = phase; set.AnimTimer = 0;
        }
        UpdateInputsUI();
    }

    private void UpdateInputsUI()
    {
        for (int u = 0; u < 2; u++)
        {
            var set = GetAnimSet(u);
            for (int p = 0; p < 3; p++)
            {
                var pc = _phaseControls[u, p];
                if (pc == null) continue;
                var ph = set.GetPhase(p);
                if (!set.Loaded || ph.Frames.Count == 0)
                {
                    pc.TxtX.Text = "0"; pc.TxtY.Text = "0"; pc.TxtDur.Text = "0.00"; pc.LblFrame.Text = "0/0"; pc.ChkAnim.IsChecked = false; pc.ChkAnim.IsEnabled = false; continue;
                }
                pc.ChkAnim.IsEnabled = true; pc.ChkAnim.IsChecked = ph.Animate;
                int idx = set.CurrentFrame[p] % ph.Frames.Count;
                var frame = ph.Frames[idx];

                if (!pc.TxtX.IsFocused) { pc.TxtX.Text = frame.OffsetX.ToString(); pc.TxtX.CaretIndex = pc.TxtX.Text.Length; }
                if (!pc.TxtY.IsFocused) { pc.TxtY.Text = frame.OffsetY.ToString(); pc.TxtY.CaretIndex = pc.TxtY.Text.Length; }
                if (!pc.TxtDur.IsFocused) { pc.TxtDur.Text = frame.Duration.ToString("F2"); pc.TxtDur.CaretIndex = pc.TxtDur.Text.Length; }

                pc.LblFrame.Text = $"{set.CurrentFrame[p] + 1}/{ph.Frames.Count}";
            }
        }
    }
}